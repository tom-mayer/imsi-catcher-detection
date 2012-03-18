from settings import Provider_list, Provider_Country_list, LAC_mapping, ARFCN_mapping, LAC_threshold

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
                return item.get_neighbour_arfcn()

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
                    if ARFCN_mapping[station.provider][0] < station.arfcn < ARFCN_mapping[station.provider][1]:
                        result = RuleResult.OK
        return result

class LACMappingRule (Rule):
    identifier = 'LAC Mapping'

    def check(self, arfcn, base_station_list):
        result = RuleResult.CRITICAL

        for station in base_station_list:
            if station.arfcn == arfcn:
                if station.provider in LAC_mapping:
                    if LAC_mapping[station.provider][0] < station.lac < LAC_mapping[station.provider][1]:
                        result = RuleResult.OK
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

        if lower_bound < lac_to_test < upper_bound:
            return RuleResult.OK
        else:
            return RuleResult.CRITICAL


class NeighbourhoodStructureRule (Rule):
    identifier = 'Neighbourhood Structure'

    def check(self, arfcn, base_station_list):
        #TODO: remove this when parser fully implemented
        if not 0 < arfcn < 125:
            return RuleResult.IGNORE

        neighbours = self._extract_neighbours(arfcn, base_station_list)

        if not len(neighbours):
            return RuleResult.CRITICAL

        at_least_one_neighbour_found = False

        for item in base_station_list:
            if item.arfcn in neighbours:
                at_least_one_neighbour_found = True
                break

        if at_least_one_neighbour_found:
            return RuleResult.OK
        else:
            return RuleResult.CRITICAL


class PureNeighbourhoodRule (Rule):
    identifier = 'Pure Neighbourhoods'

    def check(self, arfcn, base_station_list):
        #TODO: remove this when parser fully implemented
        if not 0 < arfcn < 125:
            return RuleResult.IGNORE

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
        #TODO: remove this when parser fully implemented
        if not 0 < arfcn < 125:
            return RuleResult.IGNORE

        neighbours = self._extract_neighbours(arfcn, base_station_list)
        all_neighbours_discovered = True
        for item in base_station_list:
            if item.arfcn in neighbours:
                neighbours.remove(item.arfcn)

        if len(neighbours):
            return RuleResult.WARNING
        else:
            return RuleResult.OK

class LocationAreaDatabaseRule(Rule):
    identifier = 'Location Area Database'

    def check(self, arfcn, base_station_list):
        pass

class CellIDDatabaseRule (Rule):
    identifier = 'CellID Database'

    def check(self, arfcn, base_station_list):
        pass

class BDDLearningRule (Rule):
    identifier = 'BDD Learning'

    def check(self, arfcn, base_station_list):
        pass
