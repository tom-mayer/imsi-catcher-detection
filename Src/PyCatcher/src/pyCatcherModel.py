import datetime
import gtk
import math

class BaseStationInformation: 
    #TODO: examine plmn permitted byte
    #TODO: examine emergency call capability
    def __init__ (self):
        self.country = 'Nowhere'
        self.provider = 'Carry'
        self.arfcn = 0
        self.rxlev = 0
        self.system_info_t2 = []
        self.discovery_time = datetime.datetime.now().strftime('%T')
        self.found = False
        
    def get_list_model(self):
        return (self.provider, str(self.arfcn), str(self.rxlev), self.discovery_time)
    
    def get_neighbour_arfcn(self):
        #TODO: implement this for bis/tar sysinfo
        neighbours = self.system_info_t2[3:19]
        bin_representation = ''
        neighbour_arfcn = []
        
        for value in neighbours:
            bin_representation += str(bin(int(value, 16))[2:].zfill(8))
        
        '''
        >>> for i, bit in enumerate(reversed(a)):
...     if bit == '1':
...             print i
        '''
        
        for x in xrange(1,125):
            index = 0-x
            bit = bin_representation[index]
            if bit == '1':
                neighbour_arfcn.append(abs(index))
        return neighbour_arfcn
    
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
        
        if found_filter == None:
            print_neighbours = True
        else:
            print_neighbours = not found_filter.is_active
        
        if filters != None:
            for filter in filters:
                if filter.is_active:
                    filtered_list = filter.execute(filtered_list)
                    
        for station in filtered_list:
            code += str(station.arfcn) + r' [color=red]; '
            if(print_neighbours):
                for neighbour in station.get_neighbour_arfcn():
                    code += str(station.arfcn) + r' -> ' + str(neighbour) + r'; '
        #TODO: make printing the source a fixed option
        #print preamble + code + postamble
        return preamble + code + postamble
    
    def refill_store(self, store):
        store.clear()
        for item in self._base_station_list:
            store.append(item.get_list_model())
    