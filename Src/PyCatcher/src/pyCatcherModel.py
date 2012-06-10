import datetime
import gtk
import math
from cellIDDatabase import CellIDDBStatus
from cellIDDatabase import CIDDatabases
from rules import RuleResult

class BaseStationInformation:

    def __init__ (self):
        self.country = 'Nowhere'
        self.provider = 'Carry'
        self.arfcn = 0
        self.rxlev = 0
        self.times_scanned = 1
        self.system_info_t1 = []
        self.system_info_t3 = []
        self.system_info_t4 = []
        self.system_info_t2 = []
        self.system_info_t2bis = []
        self.system_info_t2ter = []
        self.neighbours = []
        self.discovery_time = datetime.datetime.now().strftime('%T')
        self.found = False
        self.bsic = ''
        self.lac = 0
        self.cell = 0
        self.rules_report = {}
        self.evaluation_report = {}
        self.evaluation = 'NYE'
        self.evaluation_by = 'NYE'
        self.latitude = 0
        self.longitude = 0
        self.db_status = CellIDDBStatus.NOT_LOOKED_UP
        self.db_provider = CIDDatabases.NONE

        self.imm_ass_hop = 0
        self.imm_ass_non_hop = 0
        self.pagings = 0
        self.pch_scan_done = False

               
    def get_list_model(self):
        return self.provider, str(self.arfcn), str(self.rxlev), str(self.cell),self.evaluation, self.discovery_time,self.times_scanned

    def create_report(self):

        pch_scan_string = 'No'
        if self.pch_scan_done:
            pch_scan_string = 'Yes'

        report_params = '''------- Base Station Parameters -----------
Country: %s
Provider: %s
ARFCN: %s
rxlev: %s
BSIC: %s
LAC: %s
Cell ID: %s
Neighbours: %s
PCH Scan done: %s
IAs (hopping): %d
IAs (non hopping): %d
Pagings (hopping/10s): %d
Latitude: %s
Longitude: %s
Database Status: %s
Database Provider: %s
Evaluation: %s\n
'''%(self.country,self.provider, self.arfcn, self.rxlev, self.bsic, self.lac,  self.cell, ', '.join(map(str,self.neighbours)),pch_scan_string,self.imm_ass_hop,self.imm_ass_non_hop,self.pagings,self.latitude,self.longitude,self.db_status, self.db_provider,self.evaluation)

        report_rules ='------- Rule Results -----------\n'
        for key in self.rules_report.keys():
            report_rules += str(key) + ': ' + str(self.rules_report[key]) + '\n'
        report_rules +='\n\n'

        report_evaluation ='------- Evaluation Report (' + self.evaluation_by + ')-----------\n'
        for key in self.evaluation_report.keys():
            report_evaluation += str(key) + ': ' + str(self.evaluation_report[key]) + '\n'
        report_evaluation +='\n\n'

        report_raw = '''------- Raw Information -----------
SystemInfo_1:       %s
SystemInfo_2:       %s
SystemInfo_2ter:    %s
SystemInfo_2bis:    %s
SystemInfo_3:       %s
SystemInfo_4:       %s

'''%('  '.join(self.system_info_t1),'  '.join(self.system_info_t2),'  '.join(self.system_info_t2ter),'  '.join(self.system_info_t2bis), '  '.join(self.system_info_t3), '  '.join(self.system_info_t4))


        return report_params + report_rules + report_evaluation + report_raw
    
class BaseStationInformationList:
    def __init__(self):
        self._base_station_list = []
        
    def add_station(self, base_station):
        base_station.found = True
        for item in self._base_station_list:
            if item.arfcn == base_station.arfcn:
                item.discovery_time = datetime.datetime.now().strftime('%T')
                item.times_scanned += 1
                item.rxlev = base_station.rxlev
                item.lac = base_station.lac
                item.cell = base_station.cell
                item.bsic = base_station.bsic
                item.neighbours = base_station.neighbours
                item.country = base_station.country
                item.provider = base_station.provider
                item.system_info_t1 = base_station.system_info_t1
                item.system_info_t3 = base_station.system_info_t3
                item.system_info_t4 = base_station.system_info_t4
                item.system_info_t2 = base_station.system_info_t2
                item.system_info_t2bis = base_station.system_info_t2bis
                item.system_info_t2ter = base_station.system_info_t2ter
                break
        else:
            self._base_station_list.append(base_station)

    def get_dot_code(self, filters=None):
        preamble = r'digraph bsnetwork { '
        postamble = r'}'
        code = ''

        filtered_list = self._get_filtered_list(filters)

        for station in filtered_list:
            if station.evaluation == RuleResult.OK:
                code += str(station.arfcn) + r' [style = filled, fillcolor = green]; '
            elif station.evaluation == RuleResult.WARNING:
                code += str(station.arfcn) + r' [style = filled, fillcolor = yellow]; '
            elif station.evaluation == RuleResult.CRITICAL:
                code += str(station.arfcn) + r' [style = filled, fillcolor = red]; '
            else:
                code += str(station.arfcn) + r' [style = filled, fillcolor = white]; '
            for neighbour in station.neighbours:
                code += str(station.arfcn) + r' -> ' + str(neighbour) + r'; '
        #TODO: make printing the source a fixed option
        #print preamble + code + postamble
        return preamble + code + postamble
    
    def refill_store(self, store, filters=None):
        store.clear()
        filtered_list = self._get_filtered_list(filters)
        for item in filtered_list:
            store.append(item.get_list_model())

    def _get_unfiltered_list(self):
        return self._base_station_list

    def _get_filtered_list(self, filters):
        filtered_list = []
        for item in self._base_station_list:
            filtered_list.append(item)
        if filters is not None:
            for filter in filters:
                if filter.is_active:
                    filtered_list = filter.execute(filtered_list)

        return filtered_list


    def create_report(self, arfcn):
        for item in self._base_station_list:
            if item.arfcn == int(arfcn):
                return item.create_report()

    def evaluate(self, rules, evaluator):
        for station in self._base_station_list:
            rule_results = {}
            for rule in rules:
                if rule.is_active:
                    rule_results[rule.identifier] = rule.check(station.arfcn, self._base_station_list)
            station.rules_report = rule_results.copy()
            station.evaluation, station.evaluation_report = evaluator.evaluate(rule_results)
            station.evaluation_by = evaluator.identifier
