from rules import RuleResult
from settings import Rule_Groups, Rule_Weights

class EvaluatorSelect:
    CONSERVATIVE = 0
    WEIGHTED = 1
    GROUP = 2

class Evaluator:

    return_type = type(RuleResult)
    identifier = 'Base Class'

    def evaluate(self, result_list):
        return RuleResult.CRITICAL, {'Evaluator Base Class':'This should not happen!'}

class ConservativeEvaluator(Evaluator):

    identifier = 'Conservative Evaluator'

    def evaluate(self, result_list):
        final_result = RuleResult.OK
        decision_rule = 'None'
        for key in result_list.keys():
            if result_list[key] == RuleResult.WARNING:
                final_result = RuleResult.WARNING
                decision_rule = key
            if result_list[key] == RuleResult.CRITICAL:
                final_result = RuleResult.CRITICAL
                decision_rule = key
                break
        return final_result, {'Decision founded on': decision_rule}



class WeightedEvaluator(Evaluator):
    identifier = 'Weighted Evaluator'

    def evaluate(self, result_list):
        for rule, evaluation in reseult_list:
            pass



class GroupEvaluator(Evaluator):
    identifier = 'Group Evaluator'

    def evaluate(self, result_list):
        group_results = []
        for group in Rule_Groups:
            group_results.append(self.evaluate_group_results(self.convert_to_group_result_list(group,result_list)))

        if group_results.count(RuleResult.CRITICAL) > 0:
            return RuleResult.CRITICAL
        elif group_results.count(RuleResult.WARNING) > 0:
            return RuleResult.WARNING
        else:
            return RuleResult.OK

    def convert_to_group_result_list(self, group, result_list):
        group_result_list = []
        for rule in group:
            group_results.append(result_list[rule])
        return group_result_list

    def evaluate_group_results(self, results):
        oks = results.count(RuleResult.OK)
        warnings = results.count(RuleResult.WARNING)
        criticals = results.count(RuleResult.CRITICAL)
        if criticals >= oks and criticals >= warnings:
            return RuleResult.CRITICAL
        elif warnings >= oks and warnings>= criticals:
            return RuleResult.WARNING
        elif oks >= criticals and oks >= warnings:
            return RuleResult.OK
        else:
            return RuleResult.IGNORE