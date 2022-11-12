#include <iostream>
#include <cstring>
#include <cassert>
#include <set>
#include "parse_sql_to_ra.h"

SQLtoRA::SQLtoRA(){
    ra_tree_root = nullptr;
}

SQLtoRA::~SQLtoRA(){
    delete ra_tree_root;
    for(auto cte: ctes){
        delete cte;
    }
};

void SQLtoRA::parse_expression(PgQuery__Node* node, Ra__Node** ra_arg, bool& has_aggregate){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_COLUMN_REF: {
            PgQuery__ColumnRef* columnRef = node->column_ref;
            Ra__Node__Attribute* attr;
            switch(columnRef->n_fields){
                case 1: {
                    if(columnRef->fields[0]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr = new Ra__Node__Attribute("*");
                    }
                    else{
                        attr = new Ra__Node__Attribute(columnRef->fields[0]->string->str);
                    }
                    break; 
                }
                case 2: {
                    if(columnRef->fields[1]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr = new Ra__Node__Attribute("*", columnRef->fields[0]->string->str);
                    }
                    else{
                        attr = new Ra__Node__Attribute(columnRef->fields[1]->string->str, columnRef->fields[0]->string->str);
                    } 
                    break;
                } 
            }
            (*ra_arg) = attr;
            return;
        }
        case PG_QUERY__NODE__NODE_A_CONST: {
            PgQuery__AConst* aConst = node->a_const;
            Ra__Node__Constant* constant;
            switch(node->a_const->val->node_case){
                case PG_QUERY__NODE__NODE_INTEGER: { 
                    constant = new Ra__Node__Constant(std::to_string(aConst->val->integer->ival), RA__CONST_DATATYPE__INT);
                    break;
                }
                case PG_QUERY__NODE__NODE_FLOAT: {
                    constant = new Ra__Node__Constant(aConst->val->float_->str, RA__CONST_DATATYPE__FLOAT);
                    break;
                }
                case PG_QUERY__NODE__NODE_STRING: {
                    constant = new Ra__Node__Constant(aConst->val->string->str, RA__CONST_DATATYPE__STRING);
                    break;
                }
                default:
                    std::cout << "error a_const" << std::endl;
            }
            (*ra_arg) = constant;
            return;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
            if(a_expr->lexpr!=nullptr){
                parse_expression(a_expr->lexpr, &(ra_expr->l_arg), has_aggregate);
            }
            if(a_expr->rexpr!=nullptr){
                parse_expression(a_expr->rexpr, &(ra_expr->r_arg), has_aggregate);
            }
            ra_expr->operator_=a_expr->name[0]->string->str;
            (*ra_arg) = ra_expr;
            return;
        }
        case PG_QUERY__NODE__NODE_FUNC_CALL: {
            PgQuery__FuncCall* func_call = node->func_call;

            Ra__Node__Func_Call* ra_func_call = new Ra__Node__Func_Call(func_call->funcname[func_call->n_funcname-1]->string->str);
            
            if(ra_func_call->func_name=="date_part"){
                ra_func_call->func_name = "extract";
            }

            if(ra_func_call->func_name=="min" || ra_func_call->func_name=="max"
                || ra_func_call->func_name=="sum" || ra_func_call->func_name=="count"
                || ra_func_call->func_name=="avg")
            {
                ra_func_call->is_aggregating = true;
                has_aggregate = true;
                if(func_call->agg_distinct==1){
                    ra_func_call->agg_distinct = true;
                }
            }
            else{
                ra_func_call->is_aggregating = false;
            }

            for(size_t i = 0; i<func_call->n_args; i++){
                Ra__Node* expr;
                parse_expression(func_call->args[i], &expr, has_aggregate);
                ra_func_call->args.push_back(expr);
            }
            if(func_call->agg_star==1){
                Ra__Node__Attribute* attr = new Ra__Node__Attribute("*");
                ra_func_call->args.push_back(attr);
            }

            (*ra_arg) = ra_func_call;
            break;
        }
        case PG_QUERY__NODE__NODE_TYPE_CAST:{
            PgQuery__TypeCast* type_cast = node->type_cast;
            Ra__Node__Type_Cast* ra_type_cast;
            if(type_cast->type_name->n_typmods>0){
                switch(type_cast->type_name->typmods[0]->a_const->val->integer->ival){
                    case 2: ra_type_cast = new Ra__Node__Type_Cast(type_cast->type_name->names[type_cast->type_name->n_names-1]->string->str, "month"); break;
                    case 4: ra_type_cast = new Ra__Node__Type_Cast(type_cast->type_name->names[type_cast->type_name->n_names-1]->string->str, "year"); break;
                    case 8: ra_type_cast = new Ra__Node__Type_Cast(type_cast->type_name->names[type_cast->type_name->n_names-1]->string->str, "day"); break;
                    default: std::cout << "type cast typmod not supported" << std::endl;
                };
            }
            else{
                ra_type_cast = new Ra__Node__Type_Cast(type_cast->type_name->names[type_cast->type_name->n_names-1]->string->str);
            }
            parse_expression(type_cast->arg, &(ra_type_cast->expression), has_aggregate);
            (*ra_arg) = ra_type_cast;
            break;
        }
        case PG_QUERY__NODE__NODE_CASE_EXPR:{
            PgQuery__CaseExpr* case_expr = node->case_expr;
            Ra__Node__Case_Expr* ra_case_expr = new Ra__Node__Case_Expr();
            // case_expr->n_args;
            for(size_t i=0; i<case_expr->n_args;i++){
                Ra__Node* when;
                Ra__Node* then;
                when = parse_where_expression(case_expr->args[i]->case_when->expr, when);
                // parse_expression(case_expr->args[i]->case_when->expr, &when, has_aggregate);
                parse_expression(case_expr->args[i]->case_when->result, &then, has_aggregate);
                Ra__Node__Case_When* case_when = new Ra__Node__Case_When(when, then);
                ra_case_expr->args.push_back(case_when);
            }
            if(case_expr->defresult!=nullptr){
                Ra__Node* else_default;
                parse_expression(case_expr->defresult, &else_default, has_aggregate);
                ra_case_expr->else_default = else_default;
            }
            (*ra_arg) = ra_case_expr;
            break;
        }
    }
}

void SQLtoRA::find_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_COLUMN_REF: {
            PgQuery__ColumnRef* columnRef = node->column_ref;
            Ra__Node__Attribute* attr;
            switch(columnRef->n_fields){
                case 1: {
                    if(columnRef->fields[0]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr = new Ra__Node__Attribute("*");
                    }
                    else{
                        attr = new Ra__Node__Attribute(columnRef->fields[0]->string->str);
                    }
                    break; 
                }
                case 2: {
                    if(columnRef->fields[1]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr = new Ra__Node__Attribute("*", columnRef->fields[0]->string->str);
                    }
                    else{
                        attr = new Ra__Node__Attribute(columnRef->fields[1]->string->str, columnRef->fields[0]->string->str);
                    } 
                    break;
                } 
            }
            attributes.push_back(attr);
            break;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            if(a_expr->lexpr!=nullptr){
                find_expression_attributes(a_expr->lexpr, attributes);
            }
            if(a_expr->rexpr != nullptr){
                find_expression_attributes(a_expr->rexpr, attributes);
            }
            break;
        }
        case PG_QUERY__NODE__NODE_FUNC_CALL: {
            PgQuery__FuncCall* func_call = node->func_call;
            for(size_t i = 0; i<func_call->n_args; i++){
                find_expression_attributes(func_call->args[i], attributes);
            }
            break;
        }
        default: break;
    }
}

Ra__Node* SQLtoRA::parse_select(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
    Ra__Node* it = subtree_root;
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();

    bool has_aggregate = false;
    if(select_stmt->n_distinct_clause>0){ // when is n_distinct_clause > 1?
        pr->distinct = true;
    }

    // loop through each select target
    for(size_t i=0; i<select_stmt->n_target_list; i++){
        PgQuery__Node* target = select_stmt->target_list[i];
        PgQuery__ResTarget* res_target = target->res_target;
        Ra__Node__Select_Expression* sel_expr = new Ra__Node__Select_Expression();
        Ra__Node* expr;
        parse_expression(res_target->val, &expr, has_aggregate);
        sel_expr->expression = expr;
        pr->args.push_back(sel_expr);

        if(strlen(res_target->name) > 0){
            sel_expr->rename=res_target->name;
        }
    }

    // if projection expressions have aggretating func calls, add group by dummy
    if(select_stmt->n_group_clause == 0 && has_aggregate){
        // add group by dummy
        Ra__Node__Group_By* group_by = new Ra__Node__Group_By(true);
        add_subtree(pr, group_by);
    }

    return pr;
}

// TODO: if is correlated, then parent should be dependent join
Ra__Node* SQLtoRA::parse_from_subquery(PgQuery__RangeSubselect* range_subselect){
    // ->subquery->select_stmt
    Ra__Node* result;
    Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(range_subselect->subquery->select_stmt));
    pr->subquery_alias = range_subselect->alias->aliasname;

    for(size_t i=0; i<range_subselect->alias->n_colnames; i++){
        pr->subquery_columns.push_back(range_subselect->alias->colnames[i]->string->str);
    }

    if(is_correlated_subquery(range_subselect->subquery->select_stmt)){
        std::cout << "correlated subquery in from clause is not supported" << std::endl;
        // result = new Ra__Node__Join(RA__JOIN__DEPENDENT_INNER_LEFT);
        // result->childNodes.push_back(pr);
    }
    else{
        result = pr;
    }
    return result;
}

Ra__Node* SQLtoRA::parse_where_subquery(PgQuery__SelectStmt* select_stmt, Ra__Node** ra_arg){
    
    Ra__Node__Expression* predicate_expr = new Ra__Node__Expression();

    Ra__Node* join;
    if(is_correlated_subquery(select_stmt)){
        join = new Ra__Node__Join(RA__JOIN__DEPENDENT_INNER_LEFT);
    }
    else{
        join = new Ra__Node__Cross_Product();
    }

    Ra__Node__Attribute* attr;
    Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(select_stmt));
    
    // selection predicate
    auto sel_expr = static_cast<Ra__Node__Select_Expression*>(pr->args[0]);
    sel_expr->rename = "a_" + std::to_string(counter); // nested select can only produce one output
    pr->subquery_alias = "r_" + std::to_string(counter);
    counter++;
    attr = new Ra__Node__Attribute(sel_expr->rename, pr->subquery_alias);
    predicate_expr->add_arg(attr);
    *ra_arg = predicate_expr;

    join->childNodes.push_back(pr);
    return join;
};

Ra__Node* SQLtoRA::parse_where_in_list(PgQuery__Node* l_expr, PgQuery__Node* r_expr, bool negated){  
    
    Ra__Node* join;
    if(negated){
        join = new Ra__Node__Join(RA__JOIN__ANTI_LEFT_DEPENDENT);
    }
    else{
        join = new Ra__Node__Join(RA__JOIN__SEMI_LEFT_DEPENDENT);
    }

    // translate x in (a,b,c) to "select attr from (values ('MAIL'), ('SHIP')) as t(attr) where t.attr=x"    
    Ra__Node__Values* values = new Ra__Node__Values();
    values->values.resize(r_expr->list->n_items);
    for(size_t i=0; i<values->values.size(); i++){
        Ra__Node* constant;
        bool dummy_has_aggregate;
        parse_expression(r_expr->list->items[i], &constant, dummy_has_aggregate);
        values->values[i] = constant;
    }
    values->alias = "values_" + std::to_string(counter);
    values->column = "a_" + std::to_string(counter);
    counter++;

    // predicate to transform "in" into "exists"
    Ra__Node__Predicate* eq_predicate = new Ra__Node__Predicate();
    eq_predicate->binaryOperator = "=";
    bool dummy_has_aggregate;
    parse_expression(l_expr, &(eq_predicate->left), dummy_has_aggregate);
    assert(r_expr->node_case==PG_QUERY__NODE__NODE_LIST);
    eq_predicate->right = new Ra__Node__Attribute(values->column, values->alias);

    // not null check; "in" operator can return null, exists only true/false
    Ra__Node__Null_Test* null_test = new Ra__Node__Null_Test();
    null_test->type = RA__NULL_TEST__IS_NOT_NULL;
    null_test->arg = eq_predicate->left;

    // bool and predicate
    auto bool_and = new Ra__Node__Bool_Predicate();
    bool_and->bool_operator = RA__BOOL_OPERATOR__AND;
    bool_and->args.push_back(eq_predicate);
    bool_and->args.push_back(null_test);

    Ra__Node__Selection* sel = new Ra__Node__Selection();
    sel->predicate = bool_and;
    sel->childNodes.push_back(values);

    Ra__Node__Projection* pr = new Ra__Node__Projection();
    pr->args.push_back(new Ra__Node__Attribute(values->column, values->alias));
    pr->childNodes.push_back(sel);

    join->childNodes.push_back(pr);
    return join;
}

Ra__Node* SQLtoRA::parse_where_in_subquery(PgQuery__SubLink* sub_link, bool negated){    
    Ra__Node* join;
    if(negated){
        join = new Ra__Node__Join(RA__JOIN__ANTI_LEFT_DEPENDENT);
    }
    else{
        join = new Ra__Node__Join(RA__JOIN__SEMI_LEFT_DEPENDENT);
    }

    Ra__Node* subquery_root = parse_select_statement(sub_link->subselect->select_stmt); 

    // predicate to transform "in" into "exists"
    Ra__Node__Predicate* eq_predicate = new Ra__Node__Predicate();
    eq_predicate->binaryOperator = "=";
    bool dummy_has_aggregate;
    parse_expression(sub_link->testexpr, &(eq_predicate->left), dummy_has_aggregate);
    auto select_expression = static_cast<Ra__Node__Select_Expression*>(static_cast<Ra__Node__Projection*>(subquery_root)->args[0]);
    eq_predicate->right = select_expression->expression;

    // not null check; "in" operator can return null, exists only true/false
    Ra__Node__Null_Test* null_test = new Ra__Node__Null_Test();
    null_test->type = RA__NULL_TEST__IS_NOT_NULL;
    null_test->arg = eq_predicate->left;

    // bool and predicate
    auto bool_and = new Ra__Node__Bool_Predicate();
    bool_and->bool_operator = RA__BOOL_OPERATOR__AND;
    bool_and->args.push_back(eq_predicate);
    bool_and->args.push_back(null_test);

    // find subquery selection node, if exists, else create new selection node
    Ra__Node** it = &subquery_root;
    while((*it)->childNodes[0]->n_children==1){
        it = &((*it)->childNodes[0]);
        if((*it)->node_case==RA__NODE__SELECTION){
            break;
        }
    }
    if((*it)->node_case==RA__NODE__SELECTION){
        auto sel = static_cast<Ra__Node__Selection*>(*it);
        // add new predicates to original predicate
        bool_and->args.push_back(sel->predicate);

        // replace old selection predicate with new predicate
        sel->predicate = bool_and;
    }
    // add selection if subquery has none
    else{
        Ra__Node__Selection* sel = new Ra__Node__Selection();
        sel->predicate = bool_and;

        sel->childNodes.push_back((*it)->childNodes[0]);
        (*it)->childNodes[0] = sel;
    }

    join->childNodes.push_back(subquery_root);
    return join;
};

Ra__Node* SQLtoRA::parse_where_exists_subquery(PgQuery__SelectStmt* select_stmt, bool negated){
    Ra__Node* join;
    if(is_correlated_subquery(select_stmt)){
        if(negated){
            join = new Ra__Node__Join(RA__JOIN__ANTI_LEFT_DEPENDENT);
        }
        else{
            join = new Ra__Node__Join(RA__JOIN__SEMI_LEFT_DEPENDENT);
        }
    }
    else{
        if(negated){
            join = new Ra__Node__Join(RA__JOIN__ANTI_LEFT);
        }
        else{
            join = new Ra__Node__Join(RA__JOIN__SEMI_LEFT);
        }
    }
    
    join->childNodes.push_back(parse_select_statement(select_stmt));
    return join;
};

bool SQLtoRA::is_tpch_attribute(std::string attr, std::string relation){
    std::string::size_type pos = attr.find('_');
    std::string attr_prefix;
    // if (pos != std::string::npos){
        attr_prefix = attr.substr(0, pos);
    // }
    // else{
    //     return false; // "_" not found
    // }

    if(attr_prefix=="p" && relation=="part"
        || attr_prefix=="s" && relation=="supplier"
        || attr_prefix=="ps" && relation=="partsupp"
        || attr_prefix=="c" && relation=="customer"
        || attr_prefix=="n" && relation=="nation"
        || attr_prefix=="l" && relation=="lineitem"
        || attr_prefix=="r" && relation=="region"
        || attr_prefix=="o" && relation=="orders")
    {
        return true;
    }
    return false;
}

bool SQLtoRA::is_correlated_subquery(PgQuery__SelectStmt* select_stmt){

    // get relation names + aliases in subquery from
    std::set<std::pair<std::string,std::string>> relations_aliases;
    for(size_t i=0; i<select_stmt->n_from_clause; i++){
        switch(select_stmt->from_clause[i]->node_case){
            case PG_QUERY__NODE__NODE_RANGE_VAR:{
                PgQuery__RangeVar* range_var = select_stmt->from_clause[i]->range_var;
                std::string relname = range_var->relname;
                std::string alias = range_var->alias==nullptr ? "" : range_var->alias->aliasname;
                relations_aliases.insert({relname,alias});
                break;
            }
            case PG_QUERY__NODE__NODE_JOIN_EXPR:{
                PgQuery__JoinExpr* join_expr = select_stmt->from_clause[i]->join_expr;
                PgQuery__RangeVar* l_range_var = join_expr->larg->range_var;
                std::string l_relname = l_range_var->relname;
                std::string l_alias = l_range_var->alias==nullptr ? "" : l_range_var->alias->aliasname;
                relations_aliases.insert({l_relname,l_alias});

                PgQuery__RangeVar* r_range_var = join_expr->rarg->range_var;
                std::string r_relname = r_range_var->relname;
                std::string r_alias = r_range_var->alias==nullptr ? "" : r_range_var->alias->aliasname;
                relations_aliases.insert({r_relname,r_alias});
                break;
            }
        }
    }

    // get all attributes used in subquery select and where
    std::vector<Ra__Node__Attribute*> attributes;
    for(size_t i=0; i<select_stmt->n_target_list; i++){
        find_expression_attributes(select_stmt->target_list[i]->res_target->val, attributes);
    }
    if(select_stmt->where_clause!=nullptr){
        find_where_expression_attributes(select_stmt->where_clause, attributes);
    }

    // if attribute used in select/where has not been defined in from -> correlated subquery
    for(const auto& attr: attributes){
        bool found = false;
        for(const auto& rel: relations_aliases){
            // if matching alias with relation name/alias
            if((attr->alias==rel.first) || (attr->alias.length()>0 && attr->alias==rel.second)){
                found = true;
                break;
            }
            // check tpch naming
            else if(attr->alias.length()==0 && is_tpch_attribute(attr->name,rel.first)){
                found = true;
                break;
            }
        }
        // check ctes
        for(const auto& cte: ctes){
            auto cte_pr = static_cast<Ra__Node__Projection*>(cte);
            if(attr->alias.length()>0 && attr->alias==cte_pr->subquery_alias){
                found = true;
                break;
            }
            for(auto& cte_attr: cte_pr->subquery_columns){
                if(attr->name==cte_attr){
                    found = true;
                    break;
                }
            }
            if(found) break;
        }
        if(!found) return true; // is correlated subquery
    }

    return false; // is not correlated subquery
};

void SQLtoRA::find_where_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_BOOL_EXPR: {
            PgQuery__BoolExpr* expr = node->bool_expr;
            for(size_t i=0; i<expr->n_args; i++){
                find_where_expression_attributes(expr->args[i], attributes);
            }
            break;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            find_expression_attributes(a_expr->lexpr, attributes);
            find_expression_attributes(a_expr->rexpr, attributes);
            break;
        }
        default: break;
    }
}

Ra__Node* SQLtoRA::parse_where_expression(PgQuery__Node* node, Ra__Node* ra_selection, bool sublink_negated){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_BOOL_EXPR: {
            PgQuery__BoolExpr* expr = node->bool_expr;

            std::vector<PgQuery__Node*> sublinks; // exists, in, all
            std::vector<PgQuery__Node*> non_sublinks;
            for(size_t i=0; i<expr->n_args; i++){
                if(expr->args[i]->node_case==PG_QUERY__NODE__NODE_SUB_LINK){
                    sublinks.push_back(expr->args[i]);
                }
                else{
                    non_sublinks.push_back(expr->args[i]);
                }
            }

            // parse all sublinks
            for(auto& sublink: sublinks){
                // TODO: pass negation info to sublink parsing
                if(expr->boolop==PG_QUERY__BOOL_EXPR_TYPE__NOT_EXPR){
                    parse_where_expression(sublink, ra_selection, true);
                }
                else{
                    parse_where_expression(sublink, ra_selection, false);
                }
            }

            // parse all non-sublink expressions
            if(non_sublinks.size()==0){
                // if only sublinks, then selection has no predicate
                return nullptr;
            }
            else if(non_sublinks.size()==1){
                // if only one expression left, not a boolean expression anymore, return Ra__Node__Predicate
                return parse_where_expression(non_sublinks[0], ra_selection);
            }
            else{
                Ra__Node__Bool_Predicate* p = new Ra__Node__Bool_Predicate();
                switch(expr->boolop){
                    case PG_QUERY__BOOL_EXPR_TYPE__AND_EXPR: {
                        p->bool_operator = RA__BOOL_OPERATOR__AND;
                        break;
                    }
                    case PG_QUERY__BOOL_EXPR_TYPE__OR_EXPR: {
                        p->bool_operator = RA__BOOL_OPERATOR__OR;
                        break;
                    }
                    case PG_QUERY__BOOL_EXPR_TYPE__NOT_EXPR: {
                        p->bool_operator = RA__BOOL_OPERATOR__NOT;
                        break;
                    }
                }
                for(auto& non_sublink: non_sublinks){
                    Ra__Node* predicate =  parse_where_expression(non_sublink, ra_selection);
                    if(predicate!=nullptr){
                        p->args.push_back(predicate);
                    }
                }
                return p;
            }
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            Ra__Node* ra_l_expr;
            Ra__Node* ra_r_expr;

            switch(a_expr->kind){
                case PG_QUERY__A__EXPR__KIND__AEXPR_OP:{
                    p->binaryOperator = a_expr->name[0]->string->str; 
                    break;
                }
                case PG_QUERY__A__EXPR__KIND__AEXPR_LIKE:{
                    if(a_expr->name[0]->string->str[0]=='!'){
                        p->binaryOperator = " not like ";
                    }
                    else{
                        p->binaryOperator = " like ";
                    }
                    break;
                } 
                case PG_QUERY__A__EXPR__KIND__AEXPR_BETWEEN:{
                    p->binaryOperator = " between ";
                    break;
                } 
                case PG_QUERY__A__EXPR__KIND__AEXPR_IN:{
                    std::string s(a_expr->name[0]->string->str);
                    // "in"
                    if(s == "="){
                        add_subtree(ra_selection, parse_where_in_list(a_expr->lexpr, a_expr->rexpr, false));  
                    }
                    // "not in"
                    else if(s == "<>") {
                        add_subtree(ra_selection, parse_where_in_list(a_expr->lexpr, a_expr->rexpr, true)); 
                    }
                    delete p;
                    return nullptr;
                }
                default: std::cout << "expr kind not supported" << std::endl;
            }

            // case a=b: parse left and right, return predicate for selection  
            // case uncorrelated subquery: add crossproduct(s), return predicate for selection
            // case correlated subquery: add dependent join(s), set predicate to the "higher" join
            Ra__Node* left_subquery_join = nullptr;
            Ra__Node* right_subquery_join = nullptr;

            switch(a_expr->lexpr->node_case){
                case PG_QUERY__NODE__NODE_SUB_LINK:{
                    PgQuery__SubLink* sub_link = a_expr->lexpr->sub_link;
                    left_subquery_join = parse_where_subquery(sub_link->subselect->select_stmt, &ra_l_expr);
                    add_subtree(ra_selection, left_subquery_join);
                    break;
                }
                default:{
                    bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
                    parse_expression(a_expr->lexpr, &ra_l_expr, dummy_has_aggregate);
                }
            }

            switch(a_expr->rexpr->node_case){
                case PG_QUERY__NODE__NODE_SUB_LINK:{
                    PgQuery__SubLink* sub_link = a_expr->rexpr->sub_link;
                    right_subquery_join = parse_where_subquery(sub_link->subselect->select_stmt, &ra_r_expr);
                    break;
                }
                case PG_QUERY__NODE__NODE_LIST:{
                    // between
                    Ra__Node__List* list = new Ra__Node__List();
                    list->args.resize(a_expr->rexpr->list->n_items);
                    for(size_t i=0; i<list->args.size(); i++){
                        Ra__Node* expr;
                        bool dummy_has_aggregate;
                        parse_expression(a_expr->rexpr->list->items[i], &expr, dummy_has_aggregate);
                        list->args[i]=expr;
                    }
                    ra_r_expr = list;
                    break;
                }
                default:{
                    bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
                    parse_expression(a_expr->rexpr, &ra_r_expr, dummy_has_aggregate);
                }
            }
            
            p->left = ra_l_expr;
            p->right = ra_r_expr;
            

            // left and right are subqueries
            if(left_subquery_join!=nullptr && right_subquery_join!=nullptr){
                // if both joins are dependent
                if(left_subquery_join->node_case==RA__NODE__JOIN && right_subquery_join->node_case==RA__NODE__JOIN){
                    // give predicate to "higher" dependent join
                    static_cast<Ra__Node__Join*>(left_subquery_join)->predicate = p;
                    // left join added first, is higher
                    add_subtree(ra_selection, left_subquery_join);
                    add_subtree(ra_selection, right_subquery_join);
                }
                // if left is dependent, right not
                else if(left_subquery_join->node_case==RA__NODE__JOIN){
                    // add predicate to dependent join
                    static_cast<Ra__Node__Join*>(left_subquery_join)->predicate = p;

                    // first add dependent join, then cp
                    add_subtree(ra_selection, left_subquery_join);
                    add_subtree(ra_selection, right_subquery_join);
                }
                // if right is dependent, left not
                else if(right_subquery_join->node_case==RA__NODE__JOIN){
                    // add predicate to dependent join
                    static_cast<Ra__Node__Join*>(right_subquery_join)->predicate = p;
                    
                    // first add dependent join, then cp
                    add_subtree(ra_selection, right_subquery_join);
                    add_subtree(ra_selection, left_subquery_join);
                }
                return nullptr;
            }
            // left is subquery
            else if(left_subquery_join!=nullptr){
                // if dependent join, add predicate
                if(left_subquery_join->node_case==RA__NODE__JOIN){
                    static_cast<Ra__Node__Join*>(left_subquery_join)->predicate = p;
                    // add cp/join to selection
                    add_subtree(ra_selection, left_subquery_join);
                    return nullptr;
                }
                else{
                    add_subtree(ra_selection, left_subquery_join);
                    return p;
                }
            }
            // right is subquery
            else if(right_subquery_join!=nullptr){
                // if dependent join, add predicate
                if(right_subquery_join->node_case==RA__NODE__JOIN){
                    static_cast<Ra__Node__Join*>(right_subquery_join)->predicate = p;
                    // add cp/join to selection
                    add_subtree(ra_selection, right_subquery_join);
                    return nullptr;
                }
                else{
                    add_subtree(ra_selection, right_subquery_join);
                    return p;
                }
            }
            // no subqueries
            else{
                return p;
            }
        }
        // for "Exists" and "In" subqueries
        case PG_QUERY__NODE__NODE_SUB_LINK: {
            // no predicate to return for selection, subquery is connected through join
            // add subquery to from
            PgQuery__SubLink* sub_link = node->sub_link;
            switch(sub_link->sub_link_type){
                case PG_QUERY__SUB_LINK_TYPE__EXISTS_SUBLINK:{
                    add_subtree(ra_selection, parse_where_exists_subquery(sub_link->subselect->select_stmt, sublink_negated));
                    break;
                }
                // "in"
                case PG_QUERY__SUB_LINK_TYPE__ANY_SUBLINK: {
                    add_subtree(ra_selection, parse_where_in_subquery(sub_link, sublink_negated));
                    break;
                }
                default: std::cout << "sublink type not supported" << std::endl;;
            }
            return nullptr;
        }
        case PG_QUERY__NODE__NODE_NULL_TEST:{
            PgQuery__NullTest* null_test = node->null_test;
            null_test->arg;
            Ra__Node__Null_Test* ra_null_test = new Ra__Node__Null_Test(); 
            switch(null_test->nulltesttype){
                case PG_QUERY__NULL_TEST_TYPE__IS_NULL: {
                    ra_null_test->type = RA__NULL_TEST__IS_NULL;
                    break;
                }
                case PG_QUERY__NULL_TEST_TYPE__IS_NOT_NULL: {
                    ra_null_test->type = RA__NULL_TEST__IS_NOT_NULL;
                    break;
                }
            };
            bool dummy_has_aggregate;
            parse_expression(null_test->arg, &(ra_null_test->arg), dummy_has_aggregate);
            return ra_null_test;
        }
        default: std::cout << "error parse where expr" << std::endl; return nullptr;
    }
}

Ra__Node* SQLtoRA::parse_where(PgQuery__Node* where_clause){

    // case: no where clause
    if(where_clause==nullptr){
        return nullptr;
    }  

    Ra__Node__Selection* ra_selection = new Ra__Node__Selection();
    ra_selection->predicate = parse_where_expression(where_clause, ra_selection);

    // case: if selection has no predicates (e.g. where only had exists subquery), skip selection node
    if (ra_selection->predicate == nullptr){
        return ra_selection->childNodes[0];
    }

    return ra_selection;
}

Ra__Node* SQLtoRA::parse_from(PgQuery__Node** from_clause, size_t n_from_clause){

    if(n_from_clause==0){
        // Dummy child for edge case: no from clause
        return new Ra__Node__Dummy();
    }

    // add from clause relations
    std::vector<Ra__Node*> relations;
    for(size_t j=0; j<n_from_clause; j++){
        switch(from_clause[j]->node_case){
            case PG_QUERY__NODE__NODE_RANGE_VAR:{
                PgQuery__RangeVar* from_range_var = from_clause[j]->range_var;
                Ra__Node__Relation* relation = new Ra__Node__Relation(from_range_var->relname);
                if(from_range_var->alias!=nullptr){
                    relation->alias = from_range_var->alias->aliasname;
                }
                relations.push_back(relation);
                break;
            }
            case PG_QUERY__NODE__NODE_RANGE_SUBSELECT:{
                PgQuery__RangeSubselect* range_subselect = from_clause[j]->range_subselect;
                relations.push_back(parse_from_subquery(range_subselect));
                break;
            }
            case PG_QUERY__NODE__NODE_JOIN_EXPR:{
                PgQuery__JoinExpr* join_expr = from_clause[j]->join_expr;
                relations.push_back(parse_from_join(join_expr));
                
                break;
            }
        }
    }

    if(relations.size()==1){
        return relations[0];
    }

    Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();

    for(size_t i=0; i<relations.size(); i++){
        add_subtree(cp, relations[i]);
        if(i<relations.size()-2){
            add_subtree(cp, new Ra__Node__Cross_Product());
        }
    }

    return cp;
}

Ra__Node* SQLtoRA::parse_from_join(PgQuery__JoinExpr* join_expr){
    Ra__Node__Join* join;

    switch(join_expr->jointype){
        case PG_QUERY__JOIN_TYPE__JOIN_INNER:{
            join = new Ra__Node__Join(RA__JOIN__INNER);
            break;
        }
        case PG_QUERY__JOIN_TYPE__JOIN_LEFT:{
            join = new Ra__Node__Join(RA__JOIN__LEFT);
            break;
        }
        default: std::cout << "join type not supported yet" << std::endl;
    }

    if(join_expr->alias!=nullptr){
        join->alias = join_expr->alias->aliasname;
        for(size_t i=0; i<join_expr->alias->n_colnames; i++){
            join->columns.push_back(join_expr->alias->colnames[i]->string->str);
        }
    }

    // left join expression
    PgQuery__RangeVar* l_range_var = join_expr->larg->range_var;
    Ra__Node__Relation* l_relation = new Ra__Node__Relation(l_range_var->relname);
    if(l_range_var->alias!=nullptr){
        l_relation->alias = l_range_var->alias->aliasname;
    }
    join->childNodes.push_back(l_relation);
           
    // right join expression
    PgQuery__RangeVar* r_range_var = join_expr->rarg->range_var;
    Ra__Node__Relation* r_relation = new Ra__Node__Relation(r_range_var->relname);
    if(r_range_var->alias!=nullptr){
        r_relation->alias = r_range_var->alias->aliasname;
    }
    join->childNodes.push_back(r_relation);
    
    // join predicate
    Ra__Node* dummy;
    if(join_expr->quals!=nullptr){
        join->predicate = parse_where_expression(join_expr->quals, dummy);
    }

    return join;
}

void SQLtoRA::add_subtree(Ra__Node* base, Ra__Node* subtree){
    Ra__Node* it = base;
    assert(find_empty_leaf(&it));
    if(it->node_case==RA__NODE__JOIN){
        it->childNodes.insert(it->childNodes.begin(), subtree);
    }
    else{
        it->childNodes.push_back(subtree);
    }
}

bool SQLtoRA::find_empty_leaf(Ra__Node** it){
    if((*it)->n_children == 0){
        return false;
    }

    if((*it)->childNodes.size()<(*it)->n_children){
        return true;
    }

    bool found = false;
    std::vector<Ra__Node*> childNodes = (*it)->childNodes;
    for(auto child: childNodes){
        if(!found){
            *it = child;
            found = find_empty_leaf(it);
        }
    }
    return found;
}

Ra__Node* SQLtoRA::parse_order_by(PgQuery__Node** sort_clause, size_t n_sort_clause){

    if(n_sort_clause == 0){
        return nullptr;
    }

    Ra__Node__Order_By* order_by = new Ra__Node__Order_By();
    for(size_t i=0; i<n_sort_clause; i++){
        bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
        Ra__Node* ra_expr;
        parse_expression(sort_clause[i]->sort_by->node, &ra_expr, dummy_has_aggregate);
        order_by->args.push_back(ra_expr);
        switch(sort_clause[i]->sort_by->sortby_dir){
            case PG_QUERY__SORT_BY_DIR__SORTBY_DEFAULT:{
                order_by->directions.push_back(RA__ORDER_BY__DEFAULT);
                break;
            }
            case PG_QUERY__SORT_BY_DIR__SORTBY_ASC:{
                order_by->directions.push_back(RA__ORDER_BY__ASC);
                break;
            }
            case PG_QUERY__SORT_BY_DIR__SORTBY_DESC:{
                order_by->directions.push_back(RA__ORDER_BY__DESC);
                break;
            }
        }
    }
    return order_by;
}

Ra__Node* SQLtoRA::parse_group_by(PgQuery__Node** group_clause, size_t n_group_clause){
    if(n_group_clause == 0){
        return nullptr;
    }

    Ra__Node__Group_By* group_by = new Ra__Node__Group_By(false);
    for(size_t i=0; i<n_group_clause; i++){
        Ra__Node* ra_expr;
        bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
        
        parse_expression(group_clause[i], &ra_expr, dummy_has_aggregate);
        group_by->args.push_back(ra_expr);
    }

    return group_by;
}

Ra__Node* SQLtoRA::parse_having(PgQuery__Node* having_clause){
    if(having_clause==nullptr){
        return nullptr;
    }

    Ra__Node__Having* having = new Ra__Node__Having();
    Ra__Node* predicate = parse_where_expression(having_clause, having);
    if(predicate==nullptr){
        return nullptr;
    }

    having->predicate = predicate;
    return having;
}

void SQLtoRA::parse_with(PgQuery__WithClause* with_clause){
    if(with_clause!=nullptr){
        for(size_t i=0; i<with_clause->n_ctes; i++){
            // parse cte subqueries, for substitution
            PgQuery__CommonTableExpr* cte = with_clause->ctes[i]->common_table_expr;
            Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(cte->ctequery->select_stmt));
            pr->subquery_alias = cte->ctename;
            for(size_t j=0; j<cte->n_aliascolnames; j++){
                pr->subquery_columns.push_back(cte->aliascolnames[j]->string->str);
            }
            ctes.push_back(pr);
        }
    }
}

Ra__Node* SQLtoRA::parse_select_statement(PgQuery__SelectStmt* select_stmt){
    /* WITH */
    parse_with(select_stmt->with_clause);

    /* SELECT */
    Ra__Node* root = parse_select(select_stmt);

    /* ORDER BY */
    Ra__Node* sort_operator = parse_order_by(select_stmt->sort_clause, select_stmt->n_sort_clause);
    if(sort_operator != nullptr){
        // add sort underneath projection
        add_subtree(root, sort_operator);
    }

    /* HAVING */
    Ra__Node* having_operator = parse_having(select_stmt->having_clause);
    if(having_operator != nullptr){
        // add sort underneath projection
        add_subtree(root, having_operator);
    }

    /* GROUP BY */
    Ra__Node* group_by = parse_group_by(select_stmt->group_clause, select_stmt->n_group_clause);
    if(group_by != nullptr){
        // add sort underneath projection
        add_subtree(root, group_by);
    }

    /* WHERE */
    Ra__Node* selections = parse_where(select_stmt->where_clause);
    if(selections != nullptr){
        // add selections to bottom of linear subtree
        add_subtree(root, selections);
    }

    /* FROM */
    Ra__Node* cross_products = parse_from(select_stmt->from_clause, select_stmt->n_from_clause);
    if(cross_products != nullptr){
        // add cross products to first empty child ("where" could have produced cp already)
        add_subtree(root, cross_products);
    }

    return root;
};

RaTree* SQLtoRA::parse(const char* query){

    PgQueryProtobufParseResult result = pg_query_parse_protobuf(query);
    PgQuery__ParseResult* parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (const uint8_t*) result.parse_tree.data);

    // currently only supports parsing the first statement
    assert(parse_result->n_stmts==1);
    PgQuery__Node* stmt = parse_result->stmts[0]->stmt;
    ra_tree_root = parse_select_statement(stmt->select_stmt);

    return new RaTree(ra_tree_root, ctes);
}