#include <pg_query.h>
#include <iostream>
#include "parse_sql_to_ra.h"
#include "protobuf/pg_query.pb-c.h"
#include <cstring>
#include <cassert>

// return linear subtree with projections
Ra__Node* parse_select_targets(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;

    // Handle edge case: select *
    bool is_select_star = false;
    PgQuery__Node* target = select_stmt->target_list[0];
    PgQuery__ColumnRef* columnRef = target->res_target->val->column_ref;
    if(select_stmt->n_target_list==1 && columnRef->fields[0]->node_case == PG_QUERY__NODE__NODE_A_STAR){
        return subtree_root;
    }
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();
    std::vector<Ra__Node__Rename*> renames;

    // loop through each select target
    for(size_t j=0; j<select_stmt->n_target_list; j++){
        PgQuery__Node* target = select_stmt->target_list[j];
        PgQuery__ResTarget* res_target = target->res_target;

        switch(res_target->val->node_case){
            case PG_QUERY__NODE__NODE_COLUMN_REF: 
                PgQuery__ColumnRef* columnRef = res_target->val->column_ref;
                Ra__Node__Expression* expr = new Ra__Node__Expression();
                Ra__Node__Attribute* attr = new Ra__Node__Attribute();
                switch(columnRef->n_fields){
                    case 1: {
                        attr->name = columnRef->fields[0]->string->str; 
                        break;
                    }
                    case 2: {
                        attr->alias = columnRef->fields[0]->string->str; 
                        attr->name = columnRef->fields[1]->string->str;
                        break;
                    }
                }
                if(strlen(res_target->name) > 0){
                    Ra__Node__Rename* rename = new Ra__Node__Rename();
                    rename->old_name = attr->name;
                    rename->new_name = res_target->name;
                    renames.push_back(rename);
                }
                expr->consts_attributes.push_back(attr);
                pr->expressions.push_back(expr);
                break; 
            // TODO: case PG_QUERY__NODE__NODE_A_EXPR
        }
    }

    // Add renames to RA tree
    Ra__Node* temp = subtree_root;
    for(auto rename: renames){
        while(temp->childNodes.size()>0){
            temp = temp->childNodes[0];
        }
        temp->childNodes.push_back(rename);
    }

    // Add projection to RA tree
    temp = subtree_root;
    while(temp->childNodes.size()>0){
        temp = temp->childNodes[0];
    }
    temp->childNodes.push_back(pr);

    // can only return linear subtree
    assert(subtree_root->childNodes.size()==1);
    return subtree_root->childNodes[0];
}

void parse_a_expr(PgQuery__Node* expr, Ra__Node__Expression* ra_expr){
    switch(expr->node_case){
        case PG_QUERY__NODE__NODE_COLUMN_REF: {
            PgQuery__ColumnRef* columnRef = expr->column_ref;
            Ra__Node__Attribute* attr = new Ra__Node__Attribute();
            switch(columnRef->n_fields){
                case 1: {
                    attr->name = columnRef->fields[0]->string->str;
                    break; 
                }
                case 2: {
                    attr->alias = columnRef->fields[0]->string->str; 
                    attr->name = columnRef->fields[1]->string->str;
                    break;
                } 
            }
            ra_expr->consts_attributes.push_back(attr);
            break;
        }
        case PG_QUERY__NODE__NODE_A_CONST: {
            PgQuery__AConst* aConst = expr->a_const;
            Ra__Node__Constant* constValue = new Ra__Node__Constant();
            switch(expr->a_const->val->node_case){
                case 221: { // PG_QUERY__NODE__NODE_INTEGER
                    constValue->i = aConst->val->integer->ival;
                    break;
                }
                case 222: { // PG_QUERY__NODE__NODE_FLOAT
                    constValue->f = std::stof(aConst->val->float_->str);
                    break;
                }
                case 223: { // PG_QUERY__NODE__NODE_STRING
                    constValue->str = std::stof(aConst->val->string->str);
                    break;
                }
                default:
                    std::cout << "error a_const" << std::endl;
            }
            ra_expr->consts_attributes.push_back(constValue);
            break;
        }
    }
}

Ra__Node* parse_where_expression(PgQuery__Node* node){
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
                p->args.push_back(parse_where_expression(expr->args[i]));
            }
            return p;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            PgQuery__AExpr* expr = node->a_expr;
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            parse_a_expr(expr->lexpr, p->left);
            parse_a_expr(expr->rexpr, p->right);
            return p;
        }
    }
}

// return linear subtree with selections
Ra__Node* parse_where(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;

    // edge case: no where clause
    if(select_stmt->where_clause==nullptr){
        return subtree_root;
    }  

    Ra__Node__Selection* sel = new Ra__Node__Selection();
    sel->predicate = parse_where_expression(select_stmt->where_clause);

    subtree_root->childNodes.push_back(sel);
    
    // can only return linear subtree
    assert(subtree_root->childNodes.size()==1);
    return subtree_root->childNodes[0];
}

// return subtree with crossproducts & relations
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;

    // // add from clause relations
    // std::vector<std::vector<std::string>> relations;
    // for(size_t j=0; j<select_stmt->n_from_clause; j++){
    // std::vector<std::string> relation;
    // PgQuery__Node* from_clause = select_stmt->from_clause[j];
    // PgQuery__RangeVar* from_range_var = from_clause->range_var;
    // relation.push_back(from_range_var->relname);
    // relation.push_back(from_range_var->alias->aliasname);
    // relations.push_back(relation);
    // }
    // Selection* sel = (Selection*)innerNode;
    // sel->innerNode = new Relations(relations);

    // can only return linear subtree
    assert(subtree_root->childNodes.size()==1);
    return subtree_root->childNodes[0];
}

Ra__Node* parse_queries(std::vector<const char*> queries){

  std::cout << "Parsing protobuf" << std::endl;

  for(auto query: queries){
    PgQueryProtobufParseResult result;  
    result = pg_query_parse_protobuf(query);
    PgQuery__ParseResult* parse_result;
    parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (const uint8_t*) result.parse_tree.data);
    
    for(size_t i=0; i<parse_result->n_stmts; i++){
        PgQuery__RawStmt* raw_stmt = parse_result->stmts[i];
        PgQuery__Node* stmt = raw_stmt->stmt;

        Ra__Node* root = new Ra__Node();
        root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;

        /* SELECT */
        Ra__Node* projections = parse_select_targets(stmt->select_stmt);
        root->childNodes.push_back(projections);
        

        /* WHERE */
        Ra__Node* selections = parse_where(stmt->select_stmt);

        // add selections to bottom of linear subtree
        Ra__Node* temp = root;
        while(temp->childNodes.size()>0){
            temp = temp->childNodes[0];
        }
        temp->childNodes.push_back(selections);

        
        // /* FROM */
        // Ra__Node* cross_products = parse_from(stmt->select_stmt);
        
        // // add cross products to bottom of linear subtree
        // while(temp->childNodes.size()>0){
        //     temp = temp->childNodes[0];
        // }
        // temp->childNodes.push_back(cross_products);

        return root;
    }
  }
}