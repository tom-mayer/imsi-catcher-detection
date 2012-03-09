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
        self.evaluation = 'NYE'
               
    def get_list_model(self):
        return self.provider, str(self.arfcn), str(self.rxlev), self.evaluation, self.discovery_time
    
    def get_neighbour_arfcn(self):
        neighbours = self.system_info_t2[3:19]
        bin_representation = ''
        neighbour_arfcn = []
        
        for value in neighbours:
            bin_representation += str(bin(int(value, 16))[2:].zfill(8))
        

        # for i, bit in enumerate(reversed(a)):
        #     if bit == '1':
        #        print i

        for x in xrange(1,125):
            index = 0-x
            bit = bin_representation[index]
            if bit == '1':
                neighbour_arfcn.append(abs(index))
        return neighbour_arfcn
    
    def get_neighbour_arfcn_ter(self):
        pass
        
    def get_neighbour_arfcn_bis(self):
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
            report_rules += str(key) + ': ' + str(self.rules_report[key])
        report_rules +='\n\n'
        report_raw = '''------- Raw Information -----------
SystemInfo_2:       %s
SystemInfo_2ter:    %s
SystemInfo_2bis:    %s
'''%('  '.join(self.system_info_t2), '  '.join(self.system_info_t2ter), '  '.join(self.system_info_t2bis))
        return report_params + report_rules + report_raw
    
class BaseStationInformationList:
    def __init__(self):
        self._base_station_list = []
        
    def add_station(self, base_station):
        for item in self._base_station_list:
            if item.arfcn == base_station.arfcn:
                item.discovery_time = datetime.datetime.now().strftime('%T')
                break
        else:
            self._base_station_list.append(base_station)
    
    def get_dot_code(self, filters=None, found_filter=None):
        preamble = r'digraph bsnetwork { '
        postamble = r'}'
        code = ''
        filtered_list = self._base_station_list
        
        if found_filter is None:
            print_neighbours = True
        else:
            print_neighbours = not found_filter.is_active
        
        if filters is not None:
            for filter in filters:
                if filter.is_active:
                    filtered_list = filter.execute(filtered_list)
                    
        for station in filtered_list:
            code += str(station.arfcn) + r' [color=red]; '
            if print_neighbours:
                for neighbour in station.get_neighbour_arfcn():
                    code += str(station.arfcn) + r' -> ' + str(neighbour) + r'; '
        #TODO: make printing the source a fixed option
        #print preamble + code + postamble
        return preamble + code + postamble
    
    def refill_store(self, store):
        store.clear()
        for item in self._base_station_list:
            store.append(item.get_list_model())
            
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
            station.evaluation = evaluator.evaluate(rule_results)
    