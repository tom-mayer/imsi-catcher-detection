from pyCatcherModel import BaseStationInformation
import subprocess
import threading 
import re
from settings import Commands, PCH_retries
import time
import gtk
import datetime
import thread
from threading import Timer
import os
import signal
import select

class DriverConnector:        
    def __init__ (self):
        self._scan_thread_break = False
        self._firmware_thread_break = False
        self._firmware_waiting_callback = None
        self._firmware_loaded_callback = None
        self._base_station_found_callback = None
        self._firmware_thread = None
        self._scan_thread = None
        self._pch_thread = None
        self._pch_callback = None
        
    def start_scanning (self, base_station_found_callback):
        self._base_station_found_callback = base_station_found_callback
        self._scan_thread = ScanThread(self._base_station_found_callback)
        self._scan_thread.start()

    def start_firmware(self, firmware_waiting_callback, firmware_loaded_callback):
        self._firmware_waiting_callback = firmware_waiting_callback
        self._firmware_loaded_callback = firmware_loaded_callback      
        self._firmware_thread = FirmwareThread(self._firmware_waiting_callback, self._firmware_loaded_callback)
        self._firmware_thread.start()

    def start_pch_scan(self, arfcn, timeout, scan_finished_callback):
        self._pch_callback = scan_finished_callback
        self._pch_thread = PCHThread(arfcn, timeout, self._pch_callback)
        self._pch_thread.start()
        
    def stop_scanning (self):
        self._scan_thread.terminate()
        
    def stop_firmware(self):
        self._firmware_thread_break = True
        
    def shutdown(self):
        if self._firmware_thread:
            self._firmware_thread.join(3)
        if self._scan_thread:
            self._scan_thread.join(3)
        if self._pch_thread:
            self._pch_thread.join(3)
        
class FirmwareThread(threading.Thread):
    def __init__(self, firmware_waiting_callback, firmware_loaded_callback):
        gtk.gdk.threads_init()
        threading.Thread.__init__(self)
        self._firmware_waiting_callback = firmware_waiting_callback
        self._firmware_loaded_callback = firmware_loaded_callback
        self._thread_break = False

    def terminate(self):
        self._thread_break = True
       
    def run(self):
        loader_process_object = subprocess.Popen(Commands['osmocon_command'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        time.sleep(3)
        self._firmware_waiting_callback()
        while not self._thread_break:
            line = loader_process_object.stdout.readline()
            #if line:
            #    print line
            if line.strip() == 'Finishing download phase':
                self._firmware_loaded_callback()
            #time.sleep(0.5)
        loader_process_object.terminate()

class ScanThread(threading.Thread):
        def __init__(self, base_station_found_callback):
            gtk.gdk.threads_init()
            threading.Thread.__init__(self)
            self._base_station_found_callback = base_station_found_callback
            self._thread_break = False
            
        def terminate(self):
            self._thread_break = True
        
        def run(self): 
            scan_process = subprocess.Popen(Commands['scan_command'], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
            time.sleep(2)
            printall = False
            while not self._thread_break:
                line = scan_process.stdout.readline()
                if line:
                    #print line
                    #sys.stdout.flush()
                    if re.search('SysInfo', line):
                        base_station = BaseStationInformation()
                        #get country
                        line = scan_process.stdout.readline()
                        match = re.search(r'Country:\s(\w+)',line)
                        if match:
                            base_station.country = match.group(1)
                        #get provider
                        line = scan_process.stdout.readline()
                        match = re.search(r'Provider:\s(.+)',line)
                        if match:
                            base_station.provider = match.group(1)
                        #get arfcn
                        line = scan_process.stdout.readline()
                        match = re.search(r'ARFCN:\s(\d+)',line)
                        if match:
                            base_station.arfcn = int(match.group(1))
                        #get cell id
                        line = scan_process.stdout.readline()
                        match = re.search(r'Cell ID:\s(\d+)',line)
                        if match:
                            base_station.cell = int(match.group(1))
                        #get lac
                        line = scan_process.stdout.readline()
                        match = re.search(r'LAC:\s(\d+)',line)
                        if match:
                            base_station.lac = int(match.group(1))
                        #get bsic
                        line = scan_process.stdout.readline()
                        match = re.search(r'BSIC:\s(\d+,\d+)',line)
                        if match:
                            base_station.bsic = match.group(1)
                        #get rxlev
                        line = scan_process.stdout.readline()
                        match = re.search(r'rxlev:\s(.\d+)',line)
                        if match:
                            base_station.rxlev = int(match.group(1))
                        line = scan_process.stdout.readline()
                        match = re.search(r'\s((\d+)\s)*',line)
                        if match:
                            base_station.neighbours = map(int,match.group(0).strip().split(' '))
                        #get si1
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI1:\s(.+)',line)
                        if match:
                            base_station.system_info_t1 = match.group(1).split(' ')
                        #get si3
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI3:\s(.+)',line)
                        if match:
                            base_station.system_info_t3 = match.group(1).split(' ')
                        #get si4
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI4:\s(.+)',line)
                        if match:
                            base_station.system_info_t4 = match.group(1).split(' ')
                        #get si2
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI2:\s(.+)',line)
                        if match:
                            base_station.system_info_t2 = match.group(1).split(' ')
                        #get si2ter
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI2ter:\s(.+)',line)
                        if match:
                            base_station.system_info_t2ter = match.group(1).split(' ')
                        #get si2bis
                        line = scan_process.stdout.readline()
                        match = re.search(r'SI2bis:\s(.+)',line)
                        if match:
                            base_station.system_info_t2bis = match.group(1).split(' ')
                        #endinfo
                        scan_process.stdout.readline()
                        
                        self._base_station_found_callback(base_station)
            scan_process.terminate()

class PCHThread(threading.Thread):
    def __init__(self, arfcn, timeout, finished_callback):
        gtk.gdk.threads_init()
        threading.Thread.__init__(self)
        self._arfcn = arfcn
        self._timeout = timeout
        self._thread_break = False
        self._scan_finished_callback = finished_callback
        self._tmsi_dict = {}

    def terminate(self):
        self._thread_break = True

    def run(self):
        pch_retries = PCH_retries
        max_scan_time = self._timeout
        arfcn = self._arfcn
        pages_found = 0
        ia_non_hop_found = 0
        ia_hop_fund = 0
        retry = False
        buffer = []

        command = Commands['pch_command'] + ['-a', str(arfcn)]
        scan_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        time.sleep(2)
        poll_obj = select.poll()
        poll_obj.register(scan_process.stdout, select.POLLIN)

        start_time = datetime.datetime.now()
        scan_time = datetime.datetime.now() - start_time

        while(True and not self._thread_break):

            if(retry):
                scan_process.terminate()
                scan_process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                poll_obj.register(scan_process.stdout, select.POLLIN)
                retry = False

            while(pch_retries > 0 and scan_time.seconds < max_scan_time and not self._thread_break):
                scan_time = datetime.datetime.now() - start_time
                poll_result = poll_obj.poll(0)
                if poll_result:
                    line = scan_process.stdout.readline()
                else:
                    line = None

                if line:
                    if 'Paging' in line:
                        pages_found += 1
                        match = re.search(r'M\((.*)\)',line)
                        if match:
                            tmsi = match.group(1)
                            if not self._tmsi_dict.has_key(tmsi):
                                self._tmsi_dict[tmsi] = 1
                            else:
                                self._tmsi_dict[tmsi] += 1
                    if 'IMM' in line:
                        if 'HOP' in line:
                            ia_hop_fund += 1
                        else:
                            ia_non_hop_found += 1
                    if 'FBSB RESP: result=255' in line:
                        if(pch_retries > 0):
                            retry = True
                        break

            if(retry):
                print 'SCAN: retry (%d)'%pch_retries
                pch_retries -= 1
            else:
                break

        if scan_process:
            scan_process.kill()

        print 'Different TMSI: %d'%len(self._tmsi_dict)
        for key, value in self._tmsi_dict.iteritems():
            print key, value

        result = {
            'Pagings': pages_found,
            'Assignments_hopping': ia_hop_fund,
            'Assignments_non_hopping': ia_non_hop_found
        }

        if not self._thread_break:
            self._scan_finished_callback((arfcn, result))

class BufferFillerThread(threading.Thread):
    def __init__(self, buffer, process):
        gtk.gdk.threads_init()
        threading.Thread.__init__(self)
        self._buffer = buffer
        self._process = process

    def run(self):
        while(True):
            self._buffer.append(self._process.stdout.readline())