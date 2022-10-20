#include <pg_query.h>
#include <iostream>
#include "parse_sql_to_ra.h"
#include "protobuf/pg_query.pb-c.h"
#include <cstring>
#include <cassert>

Ra__Node* parse_a_expr(PgQuery__Node* expr);
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt);

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
            parse_expression(a_expr->lexpr, ra_expr);
            parse_expression(a_expr->rexpr, ra_expr);
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


// return linear subtree with projections
Ra__Node* parse_select(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
    Ra__Node* it = subtree_root;
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();

    // loop through each select target
    for(size_t j=0; j<select_stmt->n_target_list; j++){
        PgQuery__Node* target = select_stmt->target_list[j];
        PgQuery__ResTarget* res_target = target->res_target;
        Ra__Node__Expression* ra_expr = new Ra__Node__Expression();
        
        parse_expression(res_target->val, ra_expr);
        pr->expressions.push_back(ra_expr);

        if(strlen(res_target->name) > 0){
            ra_expr->rename=res_target->name;
        }
    }
    return pr;
}

// returns predicate
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

            // if left/right is sub_link, manually give ra_expr an attribute with rel_temp_name.func_call_name
            // create cp, add pr=parse_select, add cp to childNodes 
            // else continue as usual
            if(a_expr->lexpr->node_case == PG_QUERY__NODE__NODE_SUB_LINK){
                PgQuery__SubLink* sub_link = a_expr->lexpr->sub_link;
                Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();
                Ra__Node__Attribute* attr = new Ra__Node__Attribute();
                Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select(sub_link->subselect->select_stmt));
                
                // selection predicate
                pr->expressions[0]->rename = "temp_attr";
                pr->subquery_alias = "temp_alias";
                attr->name = pr->expressions[0]->rename;
                attr->alias = pr->subquery_alias;
                ra_l_expr->args.push_back(attr);
                
                // selection child node
                cp->childNodes.push_back(pr);
                childNodes.push_back(cp);
            }
            else{
                parse_expression(a_expr->lexpr, ra_l_expr);
            }

            if(a_expr->rexpr->node_case == PG_QUERY__NODE__NODE_SUB_LINK){
                PgQuery__SubLink* sub_link = a_expr->rexpr->sub_link;
                Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();
                Ra__Node__Attribute* attr = new Ra__Node__Attribute();
                Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select(sub_link->subselect->select_stmt));
                Ra__Node* relations = parse_from(sub_link->subselect->select_stmt);

                // selection predicate
                pr->expressions[0]->rename = "temp_attr";
                pr->subquery_alias = "temp_alias";
                pr->childNodes.push_back(relations);
                attr->name = pr->expressions[0]->rename;
                attr->alias = pr->subquery_alias;
                ra_r_expr->args.push_back(attr);
                
                // selection child node
                cp->childNodes.push_back(pr);
                childNodes.push_back(cp);
            }
            else{
                parse_expression(a_expr->rexpr, ra_r_expr);
            }
            
            p->left = ra_l_expr;
            p->right = ra_r_expr;

            return p;
        }
        case PG_QUERY__NODE__NODE_SUB_LINK: {
            // switch case PG_QUERY__SUB_LINK_TYPE__EXPR_SUBLINK
            PgQuery__SubLink* sub_link = node->sub_link;
            Ra__Node__Cross_Product* cp = new Ra__Node__Cross_Product();
            Ra__Node__Projection* pr = static_cast<Ra__Node__Projection*>(parse_select(sub_link->subselect->select_stmt));
            pr->subquery_alias = "temp";
            cp->childNodes.push_back(pr);
            childNodes.push_back(cp);
        }
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

Ra__Node* parse_query(const char* query){

    PgQueryProtobufParseResult result;  
    result = pg_query_parse_protobuf(query);
    PgQuery__ParseResult* parse_result;
    parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (const uint8_t*) result.parse_tree.data);

    // currently only supports parsing the first statement
    for(size_t i=0; i<parse_result->n_stmts; i++){
        PgQuery__RawStmt* raw_stmt = parse_result->stmts[i];
        PgQuery__Node* stmt = raw_stmt->stmt;

        Ra__Node* root = new Ra__Node();
        Ra__Node* it = root;
        root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
        root->n_children = 1;

        /* SELECT */
        Ra__Node* projections = parse_select(stmt->select_stmt);
        root->childNodes.push_back(projections);
        

        /* WHERE */
        Ra__Node* selections = parse_where(stmt->select_stmt);
        if(selections != nullptr){
            // add selections to bottom of linear subtree
            find_empty_leaf(&it);
            it->childNodes.push_back(selections);
        }

        
        /* FROM */
        Ra__Node* cross_products = parse_from(stmt->select_stmt);
        if(cross_products != nullptr){
            // add cross products to first empty child (where could have added cp already)
            find_empty_leaf(&it);
            it->childNodes.push_back(cross_products);
        }

        std::cout << root->childNodes[0]->to_string() <<std::endl;
        return root;
    }
}