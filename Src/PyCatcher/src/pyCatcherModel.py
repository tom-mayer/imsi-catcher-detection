import datetime
import gtk
import math

class BaseStationInformation: 

    def __init__ (self):
        self.country = 'Nowhere'
        self.provider = 'Carry'
        self.arfcn = 0
        self.rxlev = 0
        self.system_info_t2 = []
        self.system_info_t2bis = []
        self.system_info_t2ter = []
        self.discovery_time = datetime.datetime.now().strftime('%T')
        self.found = False
        self.bsic = ''
        self.lac = 0
        self.cell = 0
        self.rules_report = {}
        self.evaluation_report = {}
        self.evaluation = 'NYE'
        self.evaluation_by = 'NYE'
               
    def get_list_model(self):
        return self.provider, str(self.arfcn), str(self.rxlev), self.evaluation, self.discovery_time
    
    def get_neighbour_arfcn(self):
        if 1 < self.arfcn < 125:
            return self._parse_900()
        return []

    def si_to_bin(self, si):
        neighbours = si[3:19]
        bin_representation = ''
        for value in neighbours:
            bin_representation += str(bin(int(value, 16))[2:].zfill(8))
        return bin_representation

    def parse_bit_mask(self, si, offset):
        bin_representation = self.si_to_bin(si)
        neighbours = []
        for x in xrange(1,125):
            index = 0-x
            bit = bin_representation[index]
            if bit == '1':
                neighbours.append(abs(index) + offset)
        return neighbours


    def _parse_900(self):
        neighbours = self.parse_bit_mask(self.system_info_t2, 0)
        return map(int, neighbours)

    def _parse_1800(self):
        pass

    def _parse_900_ext(self):
        pass

    def create_report(self):
        report_params = '''------- Base Station Parameters -----------
Country: %s
Provider: %s
ARFCN: %s
rxlev: %s
BSIC: %s
LAC: %s
Cell ID: %s
Neighbours: %s
Evaluation: %s\n
'''%(self.country,self.provider, self.arfcn, self.rxlev, self.bsic, self.lac,  self.cell, ', '.join(map(str,self.get_neighbour_arfcn())),self.evaluation)

        report_rules ='------- Rule Results -----------\n'
        for key in self.rules_report.keys():
            report_rules += str(key) + ': ' + str(self.rules_report[key]) + '\n'
        report_rules +='\n\n'

        report_evaluation ='------- Evaluation Report (' + self.evaluation_by + ')-----------\n'
        for key in self.evaluation_report.keys():
            report_evaluation += str(key) + ': ' + str(self.evaluation_report[key]) + '\n'
        report_evaluation +='\n\n'

        report_raw = '''------- Raw Information -----------
SystemInfo_2:       %s
SystemInfo_2ter:    %s
SystemInfo_2bis:    %s
'''%('  '.join(self.system_info_t2), '  '.join(self.system_info_t2ter), '  '.join(self.system_info_t2bis))


        return report_params + report_rules + report_evaluation + report_raw
    
class BaseStationInformationList:
    def __init__(self):
        self._base_station_list = []
        
    def add_station(self, base_station):
        base_station.found = True
        for item in self._base_station_list:
            #TODO: check if this works like i thought
            if item.arfcn == base_station.arfcn and item.bsic == base_station.bsic:
                item.discovery_time = datetime.datetime.now().strftime('%T')
                break
        else:
            self._base_station_list.append(base_station)
    
    def get_dot_code(self, band_filter, filters=None):
        preamble = r'digraph bsnetwork { '
        postamble = r'}'
        code = ''

        filtered_list = self._get_filtered_list(band_filter, filters)

        for station in filtered_list:
            code += str(station.arfcn) + r' [color=red]; '
            for neighbour in station.get_neighbour_arfcn():
                code += str(station.arfcn) + r' -> ' + str(neighbour) + r'; '
        #TODO: make printing the source a fixed option
        print preamble + code + postamble
        return preamble + code + postamble
    
    def refill_store(self, store, band_filter, filters=None):
        store.clear()
        filtered_list = self._get_filtered_list(band_filter, filters)
        for item in filtered_list:
            store.append(item.get_list_model())


    def _get_filtered_list(self, band_filter, filters):
        filtered_list = []

        #only implemented for 900 band so far, so no distinction
        if band_filter.is_active:
            for item in self._base_station_list:
                if 0 < item.arfcn < 125:
                    filtered_list.append(item)
        else:
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
    