from pyCatcherModel import BaseStationInformation
import subprocess
import threading 

class DriverConnector:
    def __init__ (self, bs_found_callback):
        self._thread_break = False
        self._bs_found_callback = bs_found_callback
        pass
        
    def start_scanning (self):
        self._thread_break = False
        threading.Thread(target= self._do_scan).start()

    def _do_scan(self):
        #TODO: insert right command here
        ps_object = subprocess.Popen('ps', stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        while not self._thread_break:
            base_station_info = BaseStationInformation()
            base_station_info.parse_file(ps_object.stdout)
            self._bs_found_callback(base_station_info)
        ps_object.kill()
    
    def stop_scanning (self):
        self._thread_break = True
        