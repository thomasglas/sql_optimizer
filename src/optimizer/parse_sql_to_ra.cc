#include <pg_query.h>
#include <iostream>
#include "parse_sql_to_ra.h"
#include "protobuf/pg_query.pb-c.h"
#include <cstring>
#include <cassert>
#include <unordered_set>

Ra__Node* parse_a_expr(PgQuery__Node* expr);
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt);
Ra__Node* parse_select_statement(PgQuery__SelectStmt* select_stmt);
Ra__Node* parse_where_expression(PgQuery__Node* node, std::vector<Ra__Node*>& childNodes);
void find_where_expression_attributes(PgQuery__Node* node, std::vector<Ra__Node__Attribute*>& attributes);

// pushes to Ra__Node__Expression const/attributes & operators
// input PgQuery__Node* = PG_QUERY__NODE__NODE_COLUMN_REF/PG_QUERY__NODE__NODE_A_CONST/PG_QUERY__NODE__NODE_A_EXPR
// select: predicate->expressions
// where: Ra__Node__Predicate->left/right
void parse_expression(PgQuery__Node* node, Ra__Node__Expression* ra_expr){
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
                parse_expression(a_expr->lexpr, ra_expr);
            }
            if(a_expr->rexpr == nullptr){
                ra_expr->args.push_back(new Ra__Node__Dummy());
            }
            else{
                parse_expression(a_expr->rexpr, ra_expr);
            }
            ra_expr->operators.push_back(a_expr->name[0]->string->str);
            return;
        }
        case PG_QUERY__NODE__NODE_FUNC_CALL: {
            PgQuery__FuncCall* func_call = node->func_call;
            assert(func_call->n_funcname==1); // in which cases is n_funcname > 1?
            Ra__Node__Func_Call* ra_func_call = new Ra__Node__Func_Call();
            ra_func_call->func_name = func_call->funcname[0]->string->str;
            ra_func_call->expression = new Ra__Node__Expression();
            for(size_t i = 0; i<func_call->n_args; i++){
                parse_expression(func_call->args[i], ra_func_call->expression);
            }
            ra_expr->args.push_back(ra_func_call);
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

// return linear subtree with projections
Ra__Node* parse_select(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
    Ra__Node* it = subtree_root;
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();

    // loop through each select target
    for(size_t i=0; i<select_stmt->n_target_list; i++){
        PgQuery__Node* target = select_stmt->target_list[i];
        PgQuery__ResTarget* res_target = target->res_target;
        Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
        
        parse_expression(res_target->val, ra_expr);
        pr->args.push_back(ra_expr);

        if(strlen(res_target->name) > 0){
            ra_expr->rename=res_target->name;
        }
    }

    // group by
    if(select_stmt->n_group_clause > 0){
        Ra__Node__Group_By* group_by = new Ra__Node__Group_By();
        for(size_t i=0; i<select_stmt->n_group_clause; i++){
            PgQuery__Node* group_clause = select_stmt->group_clause[i];
            Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
            
            parse_expression(group_clause, ra_expr);
            group_by->args.push_back(ra_expr);
        }
        if(select_stmt->having_clause!=nullptr){
            Ra__Node__Having* having = new Ra__Node__Having();
            Ra__Node__Dummy* dummy = new Ra__Node__Dummy(); 
            having->predicate = parse_where_expression(select_stmt->having_clause, dummy->childNodes);
            group_by->has_having = true;
            group_by->having = having;
        }
        pr->has_group_by = true;
        pr->group_by = group_by;
    }

    // order by
    if(select_stmt->n_sort_clause > 0){
        Ra__Node__Order_By* order_by = new Ra__Node__Order_By();
        for(size_t i=0; i<select_stmt->n_sort_clause; i++){
            PgQuery__Node* sort_clause = select_stmt->sort_clause[i];
            Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
            parse_expression(sort_clause->sort_by->node, ra_expr);
            order_by->args.push_back(ra_expr);
            switch(sort_clause->sort_by->sortby_dir){
                case PG_QUERY__SORT_BY_DIR__SORTBY_DEFAULT:
                case PG_QUERY__SORT_BY_DIR__SORTBY_ASC:{
                    order_by->directions.push_back(true);
                    break;
                }
                case PG_QUERY__SORT_BY_DIR__SORTBY_DESC:{
                    order_by->directions.push_back(false);
                    break;
                }
            }
        }
        pr->has_order_by = true;
        pr->order_by = order_by;
    }

    return pr;
}

// PgQuery__SubLink, predicate sublink side
// return cp with subtree
Ra__Node__Cross_Product* parse_subquery(PgQuery__SubLink* sub_link, Ra__Node__Expression* predicate_expr){
    Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();
    Ra__Node__Attribute* attr = new Ra__Node__Attribute();
    Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(sub_link->subselect->select_stmt));
    
    // selection predicate
    pr->args[0]->rename = "temp_attr"; // nested select can only produce one output
    pr->subquery_alias = "temp_alias";
    attr->name = pr->args[0]->rename;
    attr->alias = pr->subquery_alias;
    predicate_expr->args.push_back(attr);
    
    cp->childNodes.push_back(pr);
    return cp;
};

// PgQuery__SubLink, predicate sublink side
// return dependent join with subtree
Ra__Node__Dependent_Join* parse_correlated_subquery(PgQuery__SubLink* sub_link, Ra__Node__Expression* predicate_expr){
    
    // dependent join (w. original predicate), put correlated subquery on one side
    // add temprel.tempattr to predicate_expr

    Ra__Node__Dependent_Join* dp = new Ra__Node__Dependent_Join();
    Ra__Node__Attribute* attr = new Ra__Node__Attribute();
    Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(sub_link->subselect->select_stmt));

    pr->args[0]->rename = "temp_attr"; // nested select can only produce one output
    pr->subquery_alias = "temp_alias";
    attr->name = pr->args[0]->rename;
    attr->alias = pr->subquery_alias;

    predicate_expr->args.push_back(attr);

    // subquery will be on left side
    dp->orientation = RA__OPERATOR_ORIENTATION_RIGHT;
    dp->childNodes.push_back(pr);
    return dp;
};

// if select/where uses alias not found in from
bool is_correlated_subquery(PgQuery__SubLink* sub_link){

    PgQuery__SelectStmt* select_stmt = sub_link->subselect->select_stmt;

    // list aliases in subquery from
    std::unordered_set<std::string> aliases;
    for(size_t i=0; i<select_stmt->n_from_clause; i++){
        char* alias = select_stmt->from_clause[i]->range_var->alias->aliasname;
        // if no alias, add rel name
        if(alias!=nullptr){
            aliases.insert(alias);
        }
    }

    // check all col_ref->aliases used in subquery select and where
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
        for(const auto& alias: aliases){
            if(attr->alias==alias){
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
Ra__Node* parse_where_expression(PgQuery__Node* node, std::vector<Ra__Node*>& childNodes){
    switch(node->node_case){
        case PG_QUERY__NODE__NODE_BOOL_EXPR: {
            PgQuery__BoolExpr* expr = node->bool_expr;
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
            for(size_t i=0; i<expr->n_args; i++){
                p->args.push_back(parse_where_expression(expr->args[i], childNodes));
            }
            return p;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* a_expr = node->a_expr;
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            Ra__Node__Expression* ra_l_expr = new Ra__Node__Expression();
            Ra__Node__Expression* ra_r_expr = new Ra__Node__Expression();

            p->binaryOperator=a_expr->name[0]->string->str;

            // if subquery exists:
            // if correlated subquery: add dependent join (w. original predicate), put correlated subquery on one side
            // if normal subquery: add cp 

            // if left/right is sub_link, manually give ra_expr an attribute with rel_temp_name.func_call_name
            // create cp, add pr=parse_select, add cp to childNodes 
            // else continue as usual
            if(a_expr->lexpr->node_case == PG_QUERY__NODE__NODE_SUB_LINK){
                PgQuery__SubLink* sub_link = a_expr->lexpr->sub_link;
                if(is_correlated_subquery(sub_link)){
                    childNodes.push_back(parse_correlated_subquery(sub_link, ra_l_expr));
                }
                else{
                    childNodes.push_back(parse_subquery(sub_link, ra_l_expr));
                }
            }
            else{
                parse_expression(a_expr->lexpr, ra_l_expr);
            }

            if(a_expr->rexpr->node_case == PG_QUERY__NODE__NODE_SUB_LINK){
                PgQuery__SubLink* sub_link = a_expr->rexpr->sub_link;
                if(is_correlated_subquery(sub_link)){
                    childNodes.push_back(parse_correlated_subquery(sub_link, ra_r_expr));
                }
                else{
                    childNodes.push_back(parse_subquery(sub_link, ra_r_expr));
                }
            }
            else{
                parse_expression(a_expr->rexpr, ra_r_expr);
            }
            
            p->left = ra_l_expr;
            p->right = ra_r_expr;

            return p;
        }
        // for "Exists" and "In"
        // case PG_QUERY__NODE__NODE_SUB_LINK: {
        //     // return predicate connecting subquery to "where"
        //     // add subquery to from
        //     PgQuery__SubLink* sub_link = node->sub_link;
        //     switch(sub_link->sub_link_type){
        //         case PG_QUERY__SUB_LINK_TYPE__EXISTS_SUBLINK:{
        //             break;
        //         }
        //         // "in"
        //         case PG_QUERY__SUB_LINK_TYPE__ANY_SUBLINK: {
        //             break;
        //         }
        //     }
        //     Ra__Node__Predicate* p = new Ra__Node__Predicate();
        //     Ra__Node__Expression* ra_l_expr = new Ra__Node__Expression();
        //     Ra__Node__Expression* ra_r_expr = new Ra__Node__Expression();
        //     Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();
        //     Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select_statement(sub_link->subselect->select_stmt));

        //     // extract whole where condition
        //     // select "distinct" <connectors> from ...

        //     cp->childNodes.push_back(pr);
        //     childNodes.push_back(cp);
        //     p->binaryOperator="=";
        //     p->left = ra_l_expr;
        //     p->right = ra_r_expr;
        //     return p;
        // }
    }
}

// return linear subtree with selections
Ra__Node* parse_where(PgQuery__SelectStmt* select_stmt){

    // edge case: no where clause
    if(select_stmt->where_clause==nullptr){
        return nullptr;
    }  

    Ra__Node__Selection* sel = new Ra__Node__Selection();
    sel->predicate = parse_where_expression(select_stmt->where_clause, sel->childNodes);

    return sel;
}

// return subtree with crossproducts & relations
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt){

    if(select_stmt->n_from_clause==0){
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
    Ra__Node__Cross_Product** it = &cp;
    for(size_t i=0; i<relations.size()-2; i++){
        (*it)->childNodes.push_back(relations[i]);
        (*it)->childNodes.push_back(new Ra__Node__Cross_Product());
        *it = static_cast<Ra__Node__Cross_Product*>((*it)->childNodes[1]);
    }
    (*it)->childNodes.push_back(relations[relations.size()-2]);
    (*it)->childNodes.push_back(relations[relations.size()-1]);

    return cp;
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

Ra__Node* parse_select_statement(PgQuery__SelectStmt* select_stmt){

    /* SELECT */
    Ra__Node* projection = parse_select(select_stmt);
    
    Ra__Node* it = projection;

    /* WHERE */
    Ra__Node* selections = parse_where(select_stmt);
    if(selections != nullptr){
        // add selections to bottom of linear subtree
        find_empty_leaf(&it);
        it->childNodes.push_back(selections);
    }

    /* FROM */
    Ra__Node* cross_products = parse_from(select_stmt);
    if(cross_products != nullptr){
        // add cross products to first empty child ("where" could have produced cp already)
        find_empty_leaf(&it);
        it->childNodes.push_back(cross_products);
    }

    std::cout << projection->to_string() <<std::endl;
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
}