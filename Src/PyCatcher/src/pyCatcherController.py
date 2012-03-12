import gtk
import gtk.glade 
from driverConnector import DriverConnector
from pyCatcherModel import BaseStationInformation, BaseStationInformationList
from pyCatcherView import PyCatcherGUI
from filters import ARFCNFilter,FoundFilter,ProviderFilter
from evaluators import EvaluatorSelect, BayesEvaluator, ConservativeEvaluator, WeightedEvaluator
from rules import ProviderRule, ARFCNMappingRule, CountryMappingRule, LACMappingRule, UniqueCellIDRule, LACIntegrityRule
import pickle

class PyCatcherController:
    def __init__(self):
        self._base_station_list = BaseStationInformationList()
        store = gtk.ListStore(str,str,str,str, str)
        store.append(('-','-','-','-', '-')) 
        self.bs_tree_list_data = store
        self._gui = PyCatcherGUI(self)
        self._driver_connector = DriverConnector()       
        self._gui.log_line('GUI initialized')

        self.arfcn_filter = ARFCNFilter()
        self.provider_filter = ProviderFilter()
        #self.found_filter = FoundFilter()
        
        self._filters = [self.arfcn_filter, self.provider_filter]

        self._conservative_evaluator = ConservativeEvaluator()
        self._bayes_evaluator = BayesEvaluator()
        self._weighted_evaluator = WeightedEvaluator()
        self._active_evaluator = self._conservative_evaluator

        self.provider_rule = ProviderRule()
        self.provider_rule.is_active = True
        self.country_mapping_rule = CountryMappingRule()
        self.country_mapping_rule.is_active = True
        self.arfcn_mapping_rule = ARFCNMappingRule()
        self.arfcn_mapping_rule.is_active = True
        self.lac_mapping_rule = LACMappingRule()
        self.lac_mapping_rule.is_active = True
        self.unique_cell_id_rule = UniqueCellIDRule()
        self.unique_cell_id_rule.is_active = True

        self._rules = [self.provider_rule, self.country_mapping_rule, self.arfcn_mapping_rule, self.lac_mapping_rule,
                        self.unique_cell_id_rule]

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
        self._driver_connector.stop_firmware()
    
    def shutdown(self):
        self._driver_connector.shutdown()
    
    def _found_base_station_callback(self, base_station):
        self._gui.log_line("found " + base_station.provider + ' (' + str(base_station.arfcn) + ')')
        self._base_station_list.add_station(base_station)
        self.trigger_evaluation()
        
    def trigger_redraw(self):
        dotcode = self._base_station_list.get_dot_code(self._filters)#,self.found_filter)
        if dotcode != 'digraph bsnetwork { }':
            self._gui.load_dot(dotcode)

    def _firmware_waiting_callback(self):
        self._gui.log_line("firmware waiting for device")
        self._gui.show_info('Switch on the phone now.', 'Firmware')
        
    def _firmware_done_callback(self):
        self._gui.log_line("firmware loaded, ready for scanning")
        self._gui.show_info('Firmware load completed', 'Firmware')
        
    def fetch_report(self, arfcn):
        return self._base_station_list.create_report(arfcn)

    def set_evaluator (self, evaluator):
        if evaluator == EvaluatorSelect.CONSERVATIVE:
            self._active_evaluator = self._conservative_evaluator
        elif evaluator == EvaluatorSelect.BAYES:
            self._active_evaluator = self._bayes_evaluator
        elif evaluator == EvaluatorSelect.WEIGHTED:
            self._active_evaluator = self._weighted_evaluator

    def save_project(self, path):
        filehandler = open(path, 'w')
        pickle.dump(self._base_station_list, filehandler)
        filehandler.close()
        self._gui.log_line('Project saved to ' + path)

    def load_project(self, path):
        filehandler = open(path, 'r')
        base_station_list = pickle.load(filehandler)
        self._base_station_list = base_station_list
        self.trigger_evaluation()
        filehandler.close()
        self._gui.log_line('Project loaded from  ' + path)

    def trigger_evaluation(self):
        self._base_station_list.evaluate(self._rules, self._active_evaluator)
        self._base_station_list.refill_store(self.bs_tree_list_data)
        self.trigger_redraw()