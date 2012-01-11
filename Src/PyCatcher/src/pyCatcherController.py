import sys
import pygtk
import gtk 
import gtk.glade 
from driverConnector import DriverConnector
from pyCatcherModel import BaseStationInformation, BaseStationInformationList
from pyCatcherView import PyCatcherGUI
from filters import ARFCNFilter,FoundFilter,ProviderFilter

class PyCatcherController:
    def __init__(self):
        self._base_station_list = BaseStationInformationList()
        store = gtk.ListStore(str,str,str,str)
        store.append(('-','-','-','-')) 
        self.bs_tree_list_data = store
        self._gui = PyCatcherGUI(self)
        self._driver_connector = DriverConnector()       
        self._gui.log_line("GUI initialized")
        
        self.arfcn_filter = ARFCNFilter()       
        self.provider_filter = ProviderFilter()
        self.found_filter = FoundFilter()
        
        self._filters = [self.arfcn_filter, self.provider_filter]
        
        gtk.main()
                
    def log_message(self, message):
        self._gui.log_line(message)            
    
    def start_scan(self):
        self._gui.log_line("start scan")
        self._driver_connector.start_scanning(self._found_base_station_callback)
        
    def stop_scan(self):
        self._gui.log_line("stop scan")
        self._driver_connector.stop_scanning()
        
    def start_firmware(self):
        self._gui.log_line("start firmware")
        self._driver_connector.start_firmware(self._firmware_waiting_callback, self._firmware_done_callback)
        
    def stop_firmware(self):
        self._gui.log_line("stop firmware")
        print 'stop firmwares'
        self._driver_connector.stop_firmware()
    
    def shutdown(self):
        self._driver_connector.shutdown()
    
    def _found_base_station_callback(self, base_station):
        self._gui.log_line("found " + base_station.provider + ' (' + str(base_station.arfcn) + ')')
        self._base_station_list.add_station(base_station)        
        self._base_station_list.refill_store(self.bs_tree_list_data)
        dotcode = self._base_station_list.get_dot_code(self._filters,self.found_filter)
        self._gui.load_dot(dotcode)
        
    def trigger_redraw(self):
        dotcode = self._base_station_list.get_dot_code(self._filters,self.found_filter)
        self._gui.load_dot(dotcode)
    
    def _firmware_waiting_callback(self):
        self._gui.log_line("firmware waiting for device")
        self._gui.show_info('Switch on the phone now.', 'Firmware')
        
    def _firmware_done_callback(self):
        self._gui.log_line("firmware loaded, ready for scanning")
        self._gui.show_info('Firmware load completed', 'Firmware')
    