import gtk
import gtk.glade
import io
from driverConnector import DriverConnector
from pyCatcherModel import BaseStationInformation, BaseStationInformationList
from pyCatcherView import PyCatcherGUI
from filters import ARFCNFilter,ProviderFilter
from evaluators import EvaluatorSelect, BayesEvaluator, ConservativeEvaluator, WeightedEvaluator
from rules import ProviderRule, ARFCNMappingRule, CountryMappingRule, LACMappingRule, UniqueCellIDRule, \
    LACMedianRule, NeighbourhoodStructureRule, PureNeighbourhoodRule, FullyDiscoveredNeighbourhoodsRule, RuleResult, CellIDDatabaseRule, LocationAreaDatabaseRule
import pickle
from localAreaDatabse import LocalAreaDatabase
from cellIDDatabase import CellIDDatabase, CellIDDBStatus, CIDDatabases
from settings import Database_path

class PyCatcherController:
    def __init__(self):
        self._base_station_list = BaseStationInformationList()
        store = gtk.ListStore(str,str,str,str, str,str,str)
        store.append(('-','-','-','-', '-','-','-'))
        self.bs_tree_list_data = store
        self._gui = PyCatcherGUI(self)
        self._driver_connector = DriverConnector()       
        self._gui.log_line('GUI initialized')

        self.arfcn_filter = ARFCNFilter()
        self.provider_filter = ProviderFilter()
        
        self._filters = [self.arfcn_filter, self.provider_filter]

        self._local_area_database = LocalAreaDatabase()
        self._cell_id_database = CellIDDatabase()

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
        self.lac_median_rule = LACMedianRule()
        self.lac_median_rule.is_active = True
        self.neighbourhood_structure_rule = NeighbourhoodStructureRule()
        self.neighbourhood_structure_rule.is_active = True
        self.pure_neighbourhood_rule = PureNeighbourhoodRule()
        self.pure_neighbourhood_rule.is_active = True
        self.full_discovered_neighbourhoods_rule = FullyDiscoveredNeighbourhoodsRule()
        self.full_discovered_neighbourhoods_rule.is_active = False
        self.cell_id_db_rule = CellIDDatabaseRule()
        self.cell_id_db_rule.is_active = False
        self.location_area_database_rule =  LocationAreaDatabaseRule()
        self.location_area_database_rule.is_active = False
        self.location_area_database_rule.location_database_object = self._local_area_database

        self._rules = [self.provider_rule, self.country_mapping_rule, self.arfcn_mapping_rule, self.lac_mapping_rule,
                        self.unique_cell_id_rule, self.lac_median_rule, self.neighbourhood_structure_rule,
                        self.pure_neighbourhood_rule, self.full_discovered_neighbourhoods_rule, self.cell_id_db_rule,
                        self.location_area_database_rule]

        self.use_google = False
        self.use_open_cell_id = False
        self.use_local_db = (False, '')

        self._location = ''

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

    def update_with_web_services(self):
        self._gui.log_line('Starting online lookups...')
        for station in self._base_station_list._get_unfiltered_list():
            found = False
            if self.use_google:
                self._gui.log_line('Looking up %d on Google.'%station.cell)
                (status, lat, long) = self._cell_id_database.fetch_id_from_Google(station.cell, station.lac, station.country)
                if status == CellIDDBStatus.CONFIRMED:
                    self._gui.log_line('...found.')
                    found = True
                    station.latitude = lat
                    station.longitude = long
                    station.db_provider = CIDDatabases.GOOGLE
                station.db_status = status
            if self.use_open_cell_id and not found:
                self._gui.log_line('Looking up %d on OpenCellID.'%station.cell)
                (status, lat, long) = self._cell_id_database.fetch_id_from_OpenCellID(station.cell, station.lac, station.country, station.provider)
                if status == CellIDDBStatus.CONFIRMED:
                    self._gui.log_line('...found.')
                    found = True
                    station.latitude = lat
                    station.longitude = long
                    station.db_provider = CIDDatabases.OPENCID
                elif staus == CellIDDBStatus.APPROXIMATED:
                    self._gui.log_line('...approximated.')
                    station.latitude = lat
                    station.longitude = long
                    station.db_provider = CIDDatabases.OPENCID
                station.db_status = status
            if self.use_local_db[0] and not found:
                self._gui.log_line('Looking up %d on Local.'%station.cell)
                (status, lat, long) = self._cell_id_database.fetch_id_from_local(station.cell, self.use_local_db[1])
                if status == CellIDDBStatus.CONFIRMED:
                    self._gui.log_line('...found.')
                    station.db_provider = CIDDatabases.LOCAL
                    station.latitude = 0
                    station.longitude = 0
                station.db_status = status
        self._gui.log_line('Finished online lookups.')

    def update_location_database(self):
        self._local_area_database.load_or_create_database(self._location)
        self._local_area_database.insert_or_alter_base_stations(self._base_station_list._get_unfiltered_list())
        self._gui.log_line('Done with database upgrade on %s.'%self._location)

    def set_new_location(self,new_location):
        if new_location != self._location:
            self._location = new_location
            self._local_area_database.load_or_create_database(self._location)
            self._gui.log_line('Location changed to %s'%self._location)

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
        self._gui.log_line('Re-evaluation')
        self._base_station_list.evaluate(self._rules, self._active_evaluator)
        self._base_station_list.refill_store(self.bs_tree_list_data, self._filters)
        self.trigger_redraw()

    def trigger_redraw(self):
        dotcode = self._base_station_list.get_dot_code(self._filters)
        if dotcode != 'digraph bsnetwork { }':
            self._gui.load_dot(dotcode)
        result = RuleResult.IGNORE
        at_least_warning = False
        for item in self._base_station_list._get_filtered_list(self._filters):
           if item.evaluation == 'Ignore':
               pass
           if item.evaluation == 'Ok' and not at_least_warning:
               result = RuleResult.OK
           elif item.evaluation == 'Warning':
               result = RuleResult.WARNING
               at_least_warning = True
           elif item.evaluation == 'Critical':
               result = RuleResult.CRITICAL
               break
        self._gui.set_image(result)

    def export_csv(self):
        if self._location == '':
            self._gui.log_line('Set valid location before exporting!')
            return
        path = Database_path + self._location + '.csv'
        file = open(path,'w')
        file.write('Country, Provider, ARFCN, rxlev, BSIC, LAC, Cell ID, Evaluation, Latitude, Longitude, Encryption, DB Status, DB Provider, Neighbours\n')
        for item in self._base_station_list._get_unfiltered_list():
            file.write('%s, %s, %d, %d, %s, %d, %d, %s, %d, %d, %s, %s, %s, %s\n'%
            (item.country,
            item.provider,
            item.arfcn,
            item.rxlev,
            item.bsic.replace(',','/'),
            item.lac,
            item.cell,
            item.evaluation,
            item.latitude,
            item.longitude,
            item.encryption,
            item.db_status,
            item.db_provider,
            ' '.join(map(str,item.neighbours))))
        file.close()
        self._gui.log_line('Export done.')
