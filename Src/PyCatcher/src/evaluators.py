from rules import RuleResult

class EvaluatorSelect:
    CONSERVATIVE = 0
    WEIGHTED = 1
    BAYES = 2
    MACHINE = 3

class StationClass:
    BASE_STATION = 0
    CATCHER = 1

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

class BayesEvaluator(Evaluator):
    return_type = type(int)

class WeightedEvaluator(Evaluator):
    return_type = type(int)

class MachineLearningEvaluator(Evaluator):
    return_type = type(StationClass)