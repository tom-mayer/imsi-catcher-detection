import sys
import pygtk
import gtk #@UnresolvedImport
import gtk.glade #@UnresolvedImport
from driverConnector import DriverConnector
from pyCatcherModel import BaseStationInformation
from pyCatcherView import PyCatcherGUI

class PyCatcherController:
    def __init__(self):
        self.gui = PyCatcherGUI(self)
        gtk.main()
        self.driver_connector = DriverConnector(self._foundBaseStationCallback)
        
    def start_scan(self):
        self.driver_connector.start_scanning()
        
    def stop_scan(self):
        self.driver_connector.stop_scanning()
        
    def _foundBaseStationCallback(self):
        print "found a station"
    