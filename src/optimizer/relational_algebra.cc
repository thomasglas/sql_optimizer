#include "relational_algebra.h"

Ra__Node__Predicate::Ra__Node__Predicate() {
    node_case = Ra__Node__NodeCase::RA__NODE__PREDICATE;
    n_children = 0;
}

Ra__Node__Select_Expression::Ra__Node__Select_Expression(){
    node_case = Ra__Node__NodeCase::RA__NODE__SELECT_EXPRESSION;
    n_children = 0;
    expression = new Ra__Node__Expression();
}