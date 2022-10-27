#include <pg_query.h>
#include <iostream>
#include "parse_sql_to_ra.h"
#include "protobuf/pg_query.pb-c.h"
#include <cstring>
#include <cassert>
#include <set>

Ra__Node* parse_a_expr(PgQuery__Node* expr);
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt);
Ra__Node* parse_select_statement(PgQuery__SelectStmt* select_stmt);
Ra__Node* parse_where_expression(PgQuery__Node* node, Ra__Node sel);
void find_where_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes);
bool is_correlated_subquery(PgQuery__SubLink* sub_link);
bool find_empty_leaf(Ra__Node** it);
void add_subtree(Ra__Node* base, Ra__Node* subtree);

// pushes to Ra__Node__Expression const/attributes & operators
// input PgQuery__Node* = PG_QUERY__NODE__NODE_COLUMN_REF/PG_QUERY__NODE__NODE_A_CONST/PG_QUERY__NODE__NODE_A_EXPR
// select: predicate->expressions
// where: Ra__Node__Predicate->left/right
void parse_expression(PgQuery__Node* node, Ra__Node__Expression* ra_expr, bool& has_aggregate){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_COLUMN_REF: {
            PgQuery__ColumnRef* columnRef = node->column_ref;
            Ra__Node__Attribute* attr = new Ra__Node__Attribute();
            switch(columnRef->n_fields){
                case 1: {
                    if(columnRef->fields[0]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr->name = "*";
                    }
                    else{
                        attr->name = columnRef->fields[0]->string->str;
                    }
                    break; 
                }
                case 2: {
                    attr->alias = columnRef->fields[0]->string->str;
                    if(columnRef->fields[1]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr->name = "*";
                    }
                    else{
                        attr->name = columnRef->fields[1]->string->str;
                    } 
                    break;
                } 
            }
            ra_expr->args.push_back(attr);
            return;
        }
        case PG_QUERY__NODE__NODE_A_CONST: {
            PgQuery__AConst* aConst = node->a_const;
            Ra__Node__Constant* constant = new Ra__Node__Constant();
            switch(node->a_const->val->node_case){
                case PG_QUERY__NODE__NODE_INTEGER: { 
                    constant->i = aConst->val->integer->ival;
                    constant->dataType = RA__CONST_DATATYPE__INT;
                    break;
                }
                case PG_QUERY__NODE__NODE_FLOAT: {
                    constant->f = aConst->val->float_->str;
                    constant->dataType = RA__CONST_DATATYPE__FLOAT;
                    break;
                }
                case PG_QUERY__NODE__NODE_STRING: {
                    constant->str = aConst->val->string->str;
                    constant->dataType = RA__CONST_DATATYPE__STRING;
                    break;
                }
                default:
                    std::cout << "error a_const" << std::endl;
            }
            ra_expr->args.push_back(constant);
            return;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            if(a_expr->lexpr==nullptr){
                ra_expr->args.push_back(new Ra__Node__Dummy());
            }
            else{
                parse_expression(a_expr->lexpr, ra_expr, has_aggregate);
            }
            if(a_expr->rexpr == nullptr){
                ra_expr->args.push_back(new Ra__Node__Dummy());
            }
            else{
                parse_expression(a_expr->rexpr, ra_expr, has_aggregate);
            }
            ra_expr->operators.push_back(a_expr->name[0]->string->str);
            return;
        }
        // TODO: differentiante between aggregating func call and non aggregating 
        case PG_QUERY__NODE__NODE_FUNC_CALL: {
            PgQuery__FuncCall* func_call = node->func_call;
            assert(func_call->n_funcname==1); // in which cases is n_funcname > 1?
            Ra__Node__Func_Call* ra_func_call = new Ra__Node__Func_Call();
            
            // if func is aggregating (min,max,sum,avg,count)
            // and no group by clause -> add group by dummy

            ra_func_call->func_name = func_call->funcname[0]->string->str;
            if(ra_func_call->func_name=="min" || ra_func_call->func_name=="max"
                || ra_func_call->func_name=="sum" || ra_func_call->func_name=="count"
                || ra_func_call->func_name=="avg")
            {
                ra_func_call->is_aggregating = true;
                has_aggregate = true;
            }
            else{
                ra_func_call->is_aggregating = false;
            }

            for(size_t i = 0; i<func_call->n_args; i++){
                Ra__Node__Expression* expr = new Ra__Node__Expression();
                parse_expression(func_call->args[i], expr, has_aggregate);
                ra_func_call->args.push_back(expr);
            }
            ra_expr->args.push_back(ra_func_call);
            break;
        }
        case PG_QUERY__NODE__NODE_TYPE_CAST:{
            PgQuery__TypeCast* type_cast = node->type_cast;
            Ra__Node__Type_Cast* ra_type_cast = new Ra__Node__Type_Cast();
            ra_type_cast->type = type_cast->type_name->names[type_cast->type_name->n_names-1]->string->str;
            if(type_cast->type_name->n_typmods>0){
                switch(type_cast->type_name->typmods[0]->a_const->val->integer->ival){
                    case 2: ra_type_cast->typ_mod = "month"; break;
                    case 4: ra_type_cast->typ_mod = "year"; break;
                    case 8: ra_type_cast->typ_mod = "day"; break;
                    default: std::cout << "type cast typmod not supported" << std::endl;
                };
            }
            ra_type_cast->expression = new Ra__Node__Expression();
            parse_expression(type_cast->arg, ra_type_cast->expression, has_aggregate);
            ra_expr->args.push_back(ra_type_cast);
            break;
        }
    }
}

//find all references to table attributes in an expression. E.g. min(s.id) -> s.id
void find_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_COLUMN_REF: {
            PgQuery__ColumnRef* columnRef = node->column_ref;
            Ra__Node__Attribute* attr = new Ra__Node__Attribute();
            switch(columnRef->n_fields){
                case 1: {
                    if(columnRef->fields[0]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr->name = "*";
                    }
                    else{
                        attr->name = columnRef->fields[0]->string->str;
                    }
                    break; 
                }
                case 2: {
                    attr->alias = columnRef->fields[0]->string->str;
                    if(columnRef->fields[1]->node_case==PG_QUERY__NODE__NODE_A_STAR){
                        attr->name = "*";
                    }
                    else{
                        attr->name = columnRef->fields[1]->string->str;
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
            assert(func_call->n_funcname==1); // in which cases is n_funcname > 1?
            for(size_t i = 0; i<func_call->n_args; i++){
                find_expression_attributes(func_call->args[i], attributes);
            }
            break;
        }
        default: break;
    }
}

// returns projection
Ra__Node* parse_select(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
    Ra__Node* it = subtree_root;
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();

    bool has_aggregate = false;

    // loop through each select target
    for(size_t i=0; i<select_stmt->n_target_list; i++){
        PgQuery__Node* target = select_stmt->target_list[i];
        PgQuery__ResTarget* res_target = target->res_target;
        Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
        
        parse_expression(res_target->val, ra_expr, has_aggregate);
        pr->args.push_back(ra_expr);

        if(strlen(res_target->name) > 0){
            ra_expr->rename=res_target->name;
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

// PgQuery__SubLink, predicate sublink side
// return cp with subtree
Ra__Node* parse_subquery(PgQuery__SubLink* sub_link, Ra__Node__Expression* predicate_expr){
    
    Ra__Node* join;
    if(is_correlated_subquery(sub_link)){
        join = new Ra__Node__Join(RA__JOIN__DEPENDENT_INNER_RIGHT);
    }
    else{
        join = new Ra__Node__Cross_Product();
    }
    Ra__Node__Attribute* attr = new Ra__Node__Attribute();
    Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(sub_link->subselect->select_stmt));
    
    // selection predicate
    pr->args[0]->rename = "temp_attr"; // nested select can only produce one output
    pr->subquery_alias = "temp_alias";
    attr->name = pr->args[0]->rename;
    attr->alias = pr->subquery_alias;
    predicate_expr->args.push_back(attr);
    
    join->childNodes.push_back(pr);
    return join;
};

// returns semi join (no join predicate)
Ra__Node* parse_exists_subquery(PgQuery__SubLink* sub_link, bool negated){
    Ra__Node* join;
    if(is_correlated_subquery(sub_link)){
        if(negated){
            join = new Ra__Node__Join(RA__JOIN__ANTI_RIGHT_DEPENDENT);
        }
        else{
            join = new Ra__Node__Join(RA__JOIN__SEMI_RIGHT_DEPENDENT);
        }
    }
    else{
        if(negated){
            join = new Ra__Node__Join(RA__JOIN__ANTI_RIGHT);
        }
        else{
            join = new Ra__Node__Join(RA__JOIN__SEMI_RIGHT);
        }
    }
    
    join->childNodes.push_back(parse_select_statement(sub_link->subselect->select_stmt));
    return join;
};

bool is_tpch_attribute(std::string attr, std::string relation){
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

// if select/where uses alias not found in from
bool is_correlated_subquery(PgQuery__SubLink* sub_link){

    PgQuery__SelectStmt* select_stmt = sub_link->subselect->select_stmt;

    // get relation names + aliases in subquery from
    std::set<std::pair<std::string,std::string>> relations_aliases;
    for(size_t i=0; i<select_stmt->n_from_clause; i++){
        PgQuery__RangeVar* range_var = select_stmt->from_clause[i]->range_var;
        std::string relname = range_var->relname;
        std::string alias = range_var->alias==nullptr ? "" : range_var->alias->aliasname;
        
        relations_aliases.insert({relname,alias});
    }

    // get all attributes used in subquery select and where
    std::vector<Ra__Node__Attribute*> attributes;
    for(size_t i=0; i<select_stmt->n_target_list; i++){
        find_expression_attributes(select_stmt->target_list[i]->res_target->val, attributes);
    }
    if(select_stmt->where_clause!=nullptr){
        find_where_expression_attributes(select_stmt->where_clause, attributes);
    }

    // if attribute used in select/where was not defined in from -> correlated subquery
    for(const auto& attr: attributes){
        bool found = false;
        for(const auto& rel: relations_aliases){
            // attr.alias == rel.alias
            // OR if attr no alias: attr.name[0] = rel.name[0]
            if(attr->alias.length()>0 && attr->alias==rel.second){
                found = true;
                break;
            }
            else if(attr->alias.length()==0 && is_tpch_attribute(attr->name,rel.first)){
                found = true;
                break;
            }
        }
        if(!found) return true; // is correlated subquery
    }

    return false; // is not correlated subquery
};

// find all attributes used in where expression
void find_where_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes){
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

// parses whole where expression, returns predicate for selection
Ra__Node* parse_where_expression(PgQuery__Node* node, Ra__Node* sel, bool sublink_negated=false){
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
                    sel, parse_where_expression(sublink, sel, true);
                }
                else{
                    sel, parse_where_expression(sublink, sel, false);
                }
            }

            // parse all non-sublink expressions
            if(non_sublinks.size()==0){
                // if only sublinks, then selection has no predicate
                return nullptr;
            }
            else if(non_sublinks.size()==1){
                // if only one expression left, not a boolean expression anymore, return Ra__Node__Predicate
                return parse_where_expression(non_sublinks[0], sel);
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
                    p->args.push_back(parse_where_expression(non_sublink, sel));
                }
                return p;
            }
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            Ra__Node__Expression* ra_l_expr = new Ra__Node__Expression();
            Ra__Node__Expression* ra_r_expr = new Ra__Node__Expression();

            switch(a_expr->kind){
                case PG_QUERY__A__EXPR__KIND__AEXPR_OP:{
                    p->binaryOperator = a_expr->name[0]->string->str; 
                    break;
                }
                case PG_QUERY__A__EXPR__KIND__AEXPR_LIKE:{
                    p->binaryOperator = " like ";
                    break;
                } 
                default: std::cout << "expr kind not supported" << std::endl;
            }

            switch(a_expr->lexpr->node_case){
                case PG_QUERY__NODE__NODE_SUB_LINK:{
                    PgQuery__SubLink* sub_link = a_expr->lexpr->sub_link;
                    add_subtree(sel, parse_subquery(sub_link, ra_l_expr));
                    break;
                }
                default:{
                    bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
                    parse_expression(a_expr->lexpr, ra_l_expr, dummy_has_aggregate);
                }
            }

            switch(a_expr->rexpr->node_case){
                case PG_QUERY__NODE__NODE_SUB_LINK:{
                    PgQuery__SubLink* sub_link = a_expr->rexpr->sub_link;
                    add_subtree(sel, parse_subquery(sub_link, ra_r_expr));
                    break;
                }
                default:{
                    bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
                    parse_expression(a_expr->rexpr, ra_r_expr, dummy_has_aggregate);
                }
            }
            
            p->left = ra_l_expr;
            p->right = ra_r_expr;

            return p;
        }
        // for "Exists" and "In"
        case PG_QUERY__NODE__NODE_SUB_LINK: {
            // return predicate for selection, which connects subquery to query
            // add subquery to from
            PgQuery__SubLink* sub_link = node->sub_link;
            switch(sub_link->sub_link_type){
                case PG_QUERY__SUB_LINK_TYPE__EXISTS_SUBLINK:{
                    add_subtree(sel, parse_exists_subquery(sub_link, sublink_negated));
                    break;
                }
                // "in"
                // case PG_QUERY__SUB_LINK_TYPE__ANY_SUBLINK: {
                //     break;
                // }
                default: std::cout << "sublink type not supported" << std::endl;;
            }
            return nullptr;
        }
        default: std::cout << "error parse where expr" << std::endl; return nullptr;
    }
}

// return linear subtree with selections
Ra__Node* parse_where(PgQuery__SelectStmt* select_stmt){

    // case: no where clause
    if(select_stmt->where_clause==nullptr){
        return nullptr;
    }  

    Ra__Node__Selection* sel = new Ra__Node__Selection();
    sel->predicate = parse_where_expression(select_stmt->where_clause, sel);

    // case: selection has no predicates (e.g. where only had exists subquery)
    if (sel->predicate == nullptr){
        return sel->childNodes[0];
    }

    return sel;
}

// return subtree with crossproducts & relations
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt){

    if(select_stmt->n_from_clause==0){
        // Dummy child for edge case: no from clause
        return new Ra__Node__Dummy();
    }

    // add from clause relations
    std::vector<Ra__Node__Relation*> relations;
    for(size_t j=0; j<select_stmt->n_from_clause; j++){
        PgQuery__RangeVar* from_range_var = select_stmt->from_clause[j]->range_var;
        Ra__Node__Relation* relation = new Ra__Node__Relation();
        if(from_range_var->alias!=nullptr){
            relation->alias = from_range_var->alias->aliasname;
        }
        relation->name = from_range_var->relname;
        relations.push_back(relation);
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

void add_subtree(Ra__Node* base, Ra__Node* subtree){
    Ra__Node* it = base;
    find_empty_leaf(&it);
    assert(!it->is_full());
    it->childNodes.push_back(subtree);
}

// performs DFS to find node with empty leaf
bool find_empty_leaf(Ra__Node** it){
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

Ra__Node* parse_order_by(PgQuery__SelectStmt* select_stmt){

    if(select_stmt->n_sort_clause == 0){
        return nullptr;
    }

    Ra__Node__Order_By* order_by = new Ra__Node__Order_By();
    for(size_t i=0; i<select_stmt->n_sort_clause; i++){
        PgQuery__Node* sort_clause = select_stmt->sort_clause[i];
        bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
        Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
        parse_expression(sort_clause->sort_by->node, ra_expr, dummy_has_aggregate);
        order_by->args.push_back(ra_expr);
        switch(sort_clause->sort_by->sortby_dir){
            case PG_QUERY__SORT_BY_DIR__SORTBY_DEFAULT:
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

Ra__Node* parse_group_by(PgQuery__SelectStmt* select_stmt){
    if(select_stmt->n_group_clause == 0){
        return nullptr;
    }

    Ra__Node__Group_By* group_by = new Ra__Node__Group_By(false);
    for(size_t i=0; i<select_stmt->n_group_clause; i++){
        PgQuery__Node* group_clause = select_stmt->group_clause[i];
        Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
        bool dummy_has_aggregate; // has_aggregate used by parse_select to detect implicit group by
        
        parse_expression(group_clause, ra_expr, dummy_has_aggregate);
        group_by->args.push_back(ra_expr);
    }

    return group_by;
}

// returns having operator, potentially with subquery underneath
Ra__Node* parse_having(PgQuery__SelectStmt* select_stmt){
    
    if(select_stmt->having_clause==nullptr){
        return nullptr;
    }

    Ra__Node__Having* having = new Ra__Node__Having();
    having->predicate = parse_where_expression(select_stmt->having_clause, having);
    return having;
}

Ra__Node* parse_select_statement(PgQuery__SelectStmt* select_stmt){

    /* SELECT */
    Ra__Node* projection = parse_select(select_stmt);
    Ra__Node* it = projection;

    /* ORDER BY */
    Ra__Node* sort_operator = parse_order_by(select_stmt);
    if(sort_operator != nullptr){
        // add sort underneath projection
        add_subtree(it, sort_operator);
    }

    /* HAVING */
    Ra__Node* having_operator = parse_having(select_stmt);
    if(having_operator != nullptr){
        // add sort underneath projection
        add_subtree(it, having_operator);
    }

    /* GROUP BY */
    Ra__Node* group_by = parse_group_by(select_stmt);
    if(group_by != nullptr){
        // add sort underneath projection
        add_subtree(it, group_by);
    }

    /* WHERE */
    Ra__Node* selections = parse_where(select_stmt);
    if(selections != nullptr){
        // add selections to bottom of linear subtree
        add_subtree(it, selections);
    }

    /* FROM */
    Ra__Node* cross_products = parse_from(select_stmt);
    if(cross_products != nullptr){
        // add cross products to first empty child ("where" could have produced cp already)
        add_subtree(it, cross_products);
    }

    return projection;
};

Ra__Node* parse_sql_query(const char* query){

    PgQueryProtobufParseResult result;  
    result = pg_query_parse_protobuf(query);
    PgQuery__ParseResult* parse_result;
    parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (const uint8_t*) result.parse_tree.data);

    // currently only supports parsing the first statement
    for(size_t i=0; i<parse_result->n_stmts; i++){
        PgQuery__RawStmt* raw_stmt = parse_result->stmts[i];
        PgQuery__Node* stmt = raw_stmt->stmt;

        return parse_select_statement(stmt->select_stmt);
    }
    return nullptr;
}