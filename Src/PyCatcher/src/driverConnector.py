from pyCatcherModel import BaseStationInformation
import subprocess
import threading 
import re
from pyCatcherSettings import Commands
import time
import gtk 

class DriverConnector:        
    def __init__ (self):
        self._scan_thread_break = False
        self._firmware_thread_break = False
        self._firmware_waiting_callback = None
        self._firmware_loaded_callback = None
        self._base_station_found_callback = None
        self._firmware_thread = None
        self._scan_thread = None
        
    def start_scanning (self, base_station_found_callback):
        self._base_station_found_callback = base_station_found_callback
        self._scan_thread = ScanThread(self._base_station_found_callback)
        self._scan_thread.start()

    def start_firmware(self, firmware_waiting_callback, firmware_loaded_callback):
        self._firmware_waiting_callback = firmware_waiting_callback
        self._firmware_loaded_callback = firmware_loaded_callback      
        self._firmware_thread = FirmwareThread(self._firmware_waiting_callback, self._firmware_loaded_callback)
        self._firmware_thread.start()
        
    def stop_scanning (self):
        self._scan_thread.terminate()
        
    def stop_firmware(self):
        self._firmware_thread_break = True
        
    def shutdown(self):
        if self._firmware_thread:
            self._firmware_thread.join(3)
        if self._scan_thread:
            self._scan_thread.join(3)
        
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
        print 'killing firmware'
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
            while not self._thread_break:
                line = scan_process.stdout.readline()
                if line:
                    #print line
                    #sys.stdout.flush()
                    if re.search('SysInfo', line):
                        base_station = BaseStationInformation()
                        #get country
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'Country:\s(\w+)',line)
                        if match:
                            base_station.country = match.group(1)
                        #get provider
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'Provider:\s(.+)',line)
                        if match:
                            base_station.provider = match.group(1)
                        #get arfcn
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'ARFCN:\s(\d+)',line)
                        if match:
                            base_station.arfcn = int(match.group(1))
                        #get cell id
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'Cell ID:\s(\d+)',line)
                        if match:
                            base_station.arfcn = int(match.group(1))
                        #get lac
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'LAC:\s(\d+)',line)
                        if match:
                            base_station.lac = int(match.group(1))
                        #get bsic
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'BSIC:\s(\.+)\s',line)
                        if match:
                            base_station.bsic = int(match.group(1))
                        #get rxlev
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'rxlev\s(.\d+)',line)
                        if match:
                            base_station.rxlev = match.group(1)
                        #get si2
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'si2\s(.+)',line)
                        if match:
                            base_station.system_info_t2 = match.group(1).split(' ')
                        #get si2bis
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'si2bis\s(.+)',line)
                        if match:
                            base_station.system_info_t2bis = match.group(1).split(' ')
                        #get si2ter
                        line = line = scan_process.stdout.readline()
                        match = re.search(r'si2ter\s(.+)',line)
                        if match:
                            base_station.system_info_t2ter = match.group(1).split(' ')
                        #endinfo
                        line = line = scan_process.stdout.readline()
                        
                        self._base_station_found_callback(base_station)
            print 'killing scan'
            scan_process.terminate()
    