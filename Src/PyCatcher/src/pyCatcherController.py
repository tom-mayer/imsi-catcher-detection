import gtk
import gtk.glade
import io
from driverConnector import DriverConnector
from pyCatcherModel import BaseStationInformation, BaseStationInformationList
from pyCatcherView import PyCatcherGUI
from filters import ARFCNFilter,ProviderFilter
from evaluators import EvaluatorSelect, ConservativeEvaluator,GroupEvaluator
from rules import ProviderRule, ARFCNMappingRule, CountryMappingRule, LACMappingRule, UniqueCellIDRule, \
    LACMedianRule, NeighbourhoodStructureRule, PureNeighbourhoodRule, DiscoveredNeighboursRule, RuleResult, CellIDDatabaseRule, LocationAreaDatabaseRule, RxChangeRule, LACChangeRule,PCHRule
import pickle
from localAreaDatabse import LocalAreaDatabase
from cellIDDatabase import CellIDDatabase, CellIDDBStatus, CIDDatabases
from settings import Database_path, USR_timeout, Pagings_per_10s_threshold, Assignment_limit

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
        self._group_evaluator = GroupEvaluator()
        self._active_evaluator = self._conservative_evaluator

        self._pch_scan_running = False
        self._user_mode_flag = False
        self._remaining_pch_arfcns = []
        self._accumulated_pch_results = []
        self._pch_timeout = 10

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
        self.full_discovered_neighbourhoods_rule = DiscoveredNeighboursRule()
        self.full_discovered_neighbourhoods_rule.is_active = True
        self.cell_id_db_rule = CellIDDatabaseRule()
        self.cell_id_db_rule.is_active = False
        self.location_area_database_rule =  LocationAreaDatabaseRule()
        self.location_area_database_rule.is_active = False
        self.location_area_database_rule.location_database_object = self._local_area_database
        self.lac_change_rule = LACChangeRule()
        self.lac_change_rule.is_active = True
        self.rx_change_rule = RxChangeRule()
        self.rx_change_rule.is_active = True
        self.pch_scan_integration = PCHRule()
        self.pch_scan_integration.is_active = True

        self._rules = [self.provider_rule, self.country_mapping_rule, self.arfcn_mapping_rule, self.lac_mapping_rule,
                        self.unique_cell_id_rule, self.lac_median_rule, self.neighbourhood_structure_rule,
                        self.pure_neighbourhood_rule, self.full_discovered_neighbourhoods_rule, self.cell_id_db_rule,
                        self.location_area_database_rule, self.lac_change_rule, self.rx_change_rule, self.pch_scan_integration]

        self.use_google = False
        self.use_open_cell_id = False
        self.use_local_db = (False, '')

        self.pch_active = False
        self.sweep_active = False

        self._location = ''

        gtk.main()
                
    def log_message(self, message):
        self._gui.log_line(message)            
    
    def start_scan(self):
        if self.pch_active:
            self._gui.log_line('Cannot sweep while PCH is active')
            return
        self._gui.log_line("start scan")
        self.sweep_active = True
        self._driver_connector.start_scanning(self._found_base_station_callback)
        
    def stop_scan(self):
        if not self.sweep_active:
            return
        self._gui.log_line("stop scan")
        self.sweep_active = False
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
        elif evaluator == EvaluatorSelect.GROUP:
            self._active_evaluator = self._group_evaluator
        self.trigger_evaluation()

    def user_pch_scan(self, provider):
        if self.sweep_active:
            self._gui.log_line('Cannot PCH scan during active sweep scan.')
            return
        else:
            self.pch_active = True
        if not provider:
            self._gui.set_user_image()
            return
        else:
            self._gui.set_user_image(RuleResult.IGNORE)
        self._user_mode_flag = True
        strongest_station = None
        max_rx = -1000
        for station in self._base_station_list._get_unfiltered_list():
            if station.provider == provider:
                if station.rxlev > max_rx:
                    max_rx = station.rxlev
                    strongest_station = station
        if strongest_station:
            if strongest_station.evaluation == RuleResult.OK:
                self._remaining_pch_arfcns = [strongest_station.arfcn]
                self._accumulated_pch_results = []
                self._do_next_pch_scan()
            else:
                self._gui.set_user_image(strongest_station.evaluation)
        else:
            self._gui.set_user_image()


    def normal_pch_scan(self, arfcns, timeout):
        if self.sweep_active:
            self._gui.log_line('Cannot PCH scan during active sweep scan.')
            return
        else:
            self.pch_active = True
        self._accumulated_pch_results = []
        self._user_mode_flag = False
        self._scan_pch(arfcns, timeout)

    def _scan_pch(self, arfcns, timeout):
        self._remaining_pch_arfcns = arfcns
        self._pch_timeout = timeout
        self._do_next_pch_scan()

    def _do_next_pch_scan(self):
        if not self._remaining_pch_arfcns:
            return
        arfcn = self._remaining_pch_arfcns.pop()
        self._gui.log_line('Starting PCH scan on ARFCN %d'%arfcn)
        if self._pch_scan_running:
            return
        else:
            self._pch_scan_running = True
            self._driver_connector.start_pch_scan(arfcn, self._pch_timeout, self._pch_done_callback)

    def _pch_done_callback(self, results, pch_failed):
        arfcn, values = results

        if pch_failed:
            self._gui.log_line('PCH scan failed (%d)'%arfcn)
            if not self._user_mode_flag :
                if self._remaining_pch_arfcns:
                    self._do_next_pch_scan()
                else:
                    self._gui.set_pch_results(self._accumulated_pch_results)
            else:
                self._gui.set_user_image(RuleResult.IGNORE)
            self.pch_active = False
            return

        for station in self._base_station_list._get_unfiltered_list():
            if station.arfcn == arfcn and self.pch_scan_integration.is_active:
                station.imm_ass_non_hop = values['Assignments_non_hopping']
                station.imm_ass_hop = values['Assignments_hopping']
                station. pagings = values['Pagings']
                station.pch_scan_done = True
        self._accumulated_pch_results.append(results)
        self._gui.log_line('Finished PCH scan on ARFCN %d'%arfcn)
        self._pch_scan_running = False
        if not self._user_mode_flag :
            if self._remaining_pch_arfcns:
                self._do_next_pch_scan()
            else:
                self._gui.set_pch_results(self._accumulated_pch_results)
        else:
            arfcn, results = self._accumulated_pch_results.pop()
            if results['Assignments_non_hopping'] > 0:
                self._gui.log_line('Non hopping channel found')
                self._gui.set_user_image(RuleResult.CRITICAL)
            elif results['Assignments_hopping'] >= Assignment_limit and self._return_normalised_pagings(results['Pagings']) >= Pagings_per_10s_threshold:
                self._gui.log_line('Scan Ok')
                self._gui.set_user_image(RuleResult.OK)
            else:
                self._gui.log_line('Paging/Assignment threshold not met')
                self._gui.set_user_image(RuleResult.CRITICAL)
        self.pch_active = False

    def _return_normalised_pagings(self, pagings):
        return (float(pagings) / float(USR_timeout))*10

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
                elif status == CellIDDBStatus.APPROXIMATED:
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
        self._local_area_database.refresh_object_cache()

    def save_project(self, path):
        filehandler = open(path, 'w')
        pickle.dump(self._base_station_list, filehandler)
        filehandler.close()
        self._gui.log_line('Project saved to ' + path)

    def load_project(self, path):
        filehandler = open(path, 'r')
        base_station_list = pickle.load(filehandler)
        #bit of a hack to be able to use old scans
        for station in base_station_list._get_unfiltered_list():
            if not hasattr(station, 'pagings'):
                station.imm_ass_hop = 0
                station.imm_ass_non_hop = 0
                station.pagings = 0
                station.pch_scan_done = False
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
        self._gui.set_evaluator_image(result)

    def export_csv(self):
        if self._location == '':
            self._gui.log_line('Set valid location before exporting!')
            return
        path = Database_path + self._location + '.csv'
        file = open(path,'w')
        file.write('Country, Provider, ARFCN, rxlev, BSIC, LAC, Cell ID, Evaluation, Latitude, Longitude, DB Status, DB Provider, Neighbours\n')
        for item in self._base_station_list._get_unfiltered_list():
            file.write('%s, %s, %d, %d, %s, %d, %d, %s, %d, %d, %s, %s, %s\n'%
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
            item.db_status,
            item.db_provider,
            ' '.join(map(str,item.neighbours))))
        file.close()
        self._gui.log_line('Export done.')
