from settings import Provider_list, Provider_Country_list, LAC_mapping, ARFCN_mapping, LAC_threshold, DB_RX_threshold, \
    CH_RX_threshold
from cellIDDatabase import CellIDDBStatus
import math

class RuleResult:
    OK = 'Ok'
    WARNING = 'Warning'
    CRITICAL = 'Critical'
    IGNORE = 'Ignore'

class Rule:
    is_active = False
    identifier = 'Rule'

    def check(self, arfcn, base_station_list):
        return RuleResult.CRITICAL

    def _extract_neighbours(self, arfcn, base_station_list):
        for item in base_station_list:
            if item.arfcn == arfcn:
                return item.neighbours

    def _extract_provider(self, arfcn, base_station_list):
        for item in base_station_list:
            if item.arfcn == arfcn:
                return item.provider

class ProviderRule (Rule):
    identifier = 'Provider Check'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL

        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in Provider_list:
                    result = RuleResult.OK
                    break
        return result

class CountryMappingRule (Rule):
    identifier = 'Country Provider Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.OK

        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in Provider_Country_list:
                    if station.country != Provider_Country_list[station.provider]:
                        result = RuleResult.CRITICAL
                else:
                    result = RuleResult.CRITICAL
        return result

class ARFCNMappingRule (Rule):
    identifier = 'ARFCN Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL

        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in ARFCN_mapping:
                    for lower,upper in ARFCN_mapping[station.provider]:
                        if lower < station.arfcn < upper:
                            result = RuleResult.OK
                            break
        return result

class LACMappingRule (Rule):
    identifier = 'LAC Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL

        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in LAC_mapping:
                    for lac in LAC_mapping[station.provider]:
                        if station.lac == lac:
                            result = RuleResult.OK
                            break
        return result

class UniqueCellIDRule (Rule):
    identifier = 'Unique CellID'

    def check(self, arfcn, base_station_list):
        result = RuleResult.OK
        cell_id = 0

        for station in base_station_list:
            if station.arfcn == arfcn:
                cell_id = station.cell

        for station in base_station_list:
            if station.arfcn != arfcn:
                if station.cell == cell_id:
                    result = RuleResult.CRITICAL
        return result

class LACMedianRule (Rule):
    identifier = 'LAC Median Deviation'

    def check(self, arfcn, base_station_list):
        lac_median_list = []
        provider = self._extract_provider(arfcn, base_station_list)
        lac_to_test = 0

        for item in base_station_list:
            if arfcn == item.arfcn:
                lac_to_test = item.lac
            if provider == item.provider:
                lac_median_list.append(item.lac)

        if len(lac_median_list) < 2:
            return RuleResult.IGNORE

        lac_median_list.sort()
        median = lac_median_list[int(len(lac_median_list)/2)]
        upper_bound = median + median * LAC_threshold
        lower_bound = median - median * LAC_threshold

        if lower_bound <= lac_to_test <= upper_bound:
            return RuleResult.OK
        else:
            return RuleResult.CRITICAL


class NeighbourhoodStructureRule (Rule):
    identifier = 'Neighbourhood Structure'

    def check(self, arfcn, base_station_list):
        own_neighbours = self._extract_neighbours(arfcn, base_station_list)
        if not len(own_neighbours):
            return RuleResult.CRITICAL
        at_least_one_neighbour_found = False
        at_least_one_indirect_neighbour = False

        for item in base_station_list:
            if item.arfcn in own_neighbours:
                at_least_one_neighbour_found = True
                break
            else:
                if item.arfcn != arfcn:
                    for foreign_neighbour_arfcn in item.neighbours:
                        if foreign_neighbour_arfcn in own_neighbours:
                            at_least_one_indirect_neighbour = True

        if at_least_one_neighbour_found:
            return RuleResult.OK
        elif at_least_one_indirect_neighbour:
            return RuleResult.WARNING
        else:
            return RuleResult.CRITICAL


class PureNeighbourhoodRule (Rule):
    identifier = 'Pure Neighbourhoods'

    def check(self, arfcn, base_station_list):

        neighbours = self._extract_neighbours(arfcn, base_station_list)
        provider = self._extract_provider(arfcn, base_station_list)
        all_neighbours_pure = True
        for item in base_station_list:
            if item.arfcn in neighbours:
                if not item.provider == provider:
                    all_neighbours_pure = False

        if all_neighbours_pure:
            return RuleResult.OK
        else:
            return RuleResult.CRITICAL


class FullyDiscoveredNeighbourhoodsRule (Rule):
    identifier = 'Fully Discovered Neighbourhoods'

    def check(self, arfcn, base_station_list):

        neighbours = self._extract_neighbours(arfcn, base_station_list)
        found = 0
        for item in base_station_list:
            if item.arfcn in neighbours:
                found += 1

        if len(neighbours) != found:
            return RuleResult.CRITICAL
        else:
            return RuleResult.OK

class LocationAreaDatabaseRule(Rule):
    identifier = 'Location Area Database'
    def __init__(self):
        self.location_database_object = None

    def check(self, arfcn, base_station_list):
        if not self.location_database_object:
            return RuleResult.IGNORE
        for item in base_station_list:
            if item.arfcn == arfcn:
                result = self.location_database_object.get_station(item.cell)
                if not result:
                    return RuleResult.CRITICAL
                rxmin = result[6]
                rxmax = result[7]
                rxmin_thresh = rxmin - math.fabs(rxmin * DB_RX_threshold)
                rxmax_thresh = rxmax + math.fabs(rxmax * DB_RX_threshold)
                if rxmin_thresh <= float(item.rxlev) <= rxmax_thresh:
                    return RuleResult.OK
                else:
                    return RuleResult.CRITICAL

class CellIDDatabaseRule (Rule):
    identifier = 'CellID Database'

    def check(self, arfcn, base_station_list):
        for item in base_station_list:
            if item.arfcn == arfcn:
                if item.db_status == CellIDDBStatus.NOT_LOOKED_UP:
                    return RuleResult.IGNORE
                if item.db_status == CellIDDBStatus.CONFIRMED:
                    return RuleResult.OK
                else:
                    return RuleResult.CRITICAL

#TODO: remove debug crap
class LACChangeRule (Rule):
    identifier = 'LAC Change Rule'

    def __init__(self):
        self._old_lac = {}

    def check(self, arfcn, base_station_list):
        for item in base_station_list:
            if item.arfcn == arfcn:
                if self._old_lac.has_key(arfcn):
                    lac, old_scanned = self._old_lac[arfcn]
                    if item.times_scanned > 1:
                        if item.times_scanned > old_scanned:
                            #print 'evaluating lac change on %d(%d): old lac %d / new lac %d'%(item.times_scanned,arfcn, lac, item.lac)
                            if item.lac == lac:
                                self._old_lac[arfcn] = item.lac, item.times_scanned
                                #print '     return ok'
                                return RuleResult.OK
                            else:
                                self._old_lac[arfcn] = item.lac, item.times_scanned
                                #print '     return critical'
                                return RuleResult.CRITICAL
                        else:
                            return RuleResult.IGNORE
                    else:
                        return RuleResult.IGNORE
                else:
                    self._old_lac[arfcn] = item.lac, item.times_scanned
                    return RuleResult.IGNORE


#TODO: remove debug crap
class RxChangeRule (Rule):
    identifier = 'rx Change Rule'

    def __init__(self):
        self._old_rx = {}

    def check(self, arfcn, base_station_list):
        for item in base_station_list:
            if item.arfcn == arfcn:
                if self._old_rx.has_key(arfcn):
                    rx, old_scanned = self._old_rx[arfcn]
                    if item.times_scanned > 1:
                        if item.times_scanned > old_scanned:
                            #print 'evaluating rx change on %d(%d): old rx %d / new rx %d'%(item.times_scanned,arfcn, rx, item.rxlev)
                            lower_bound = rx - math.fabs(rx * CH_RX_threshold)
                            upper_bound = rx + math.fabs(rx * CH_RX_threshold)
                            #print '     thresholds: %d/%d'%(lower_bound, upper_bound)
                            if lower_bound <= item.rxlev <= upper_bound:
                                self._old_rx[arfcn] = item.rxlev, item.times_scanned
                                #print '     return ok'
                                return RuleResult.OK
                            else:
                                self._old_rx[arfcn] = item.rxlev, item.times_scanned
                                #print '     return critical '
                                return RuleResult.CRITICAL
                        else:
                            return RuleResult.IGNORE
                    else:
                        return RuleResult.IGNORE
                else:
                    self._old_rx[arfcn] = item.rxlev, item.times_scanned
                    return RuleResult.IGNORE
