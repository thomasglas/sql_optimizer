#include <pg_query.h>
#include <iostream>
#include "parse_sql_to_ra.h"
#include "protobuf/pg_query.pb-c.h"
#include <cstring>
#include <cassert>

Ra__Node* parse_a_expr(PgQuery__Node* expr);

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
            // ra_expr->operators.push_back();
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
                    constant->f = std::stof(aConst->val->float_->str);
                    constant->dataType = RA__CONST_DATATYPE__FLOAT;
                    break;
                }
                case PG_QUERY__NODE__NODE_STRING: {
                    constant->str = std::stof(aConst->val->string->str);
                    constant->dataType = RA__CONST_DATATYPE__STRING;
                    break;
                }
                default:
                    std::cout << "error a_const" << std::endl;
            }
            ra_expr->consts_attributes.push_back(constant);
            // ra_expr->operators.push_back();
            return;
        }
        case PG_QUERY__NODE__NODE_A_EXPR: {
            // Ra__Node__Predicate binaryOperator?
            PgQuery__AExpr* a_expr = node->a_expr;
            parse_expression(a_expr->lexpr, ra_expr);
            parse_expression(a_expr->rexpr, ra_expr);
            ra_expr->operators.push_back(a_expr->name[0]->string->str);
            return;
        }
        // TODO: case PG_QUERY__NODE__NODE_FUNC_CALL
    }
}


// return linear subtree with projections
Ra__Node* parse_select(PgQuery__SelectStmt* select_stmt){

    Ra__Node* subtree_root = new Ra__Node();
    subtree_root->node_case = Ra__Node__NodeCase::RA__NODE__ROOT;
    Ra__Node* it = subtree_root;

    // Handle edge case: select *
    if(select_stmt->n_target_list==1 &&
        select_stmt->target_list[0]->res_target->val->node_case==PG_QUERY__NODE__NODE_COLUMN_REF && 
        select_stmt->target_list[0]->res_target->val->column_ref->fields[0]->node_case == PG_QUERY__NODE__NODE_A_STAR)
    {
        return nullptr;
    }
    
    Ra__Node__Projection* pr = new Ra__Node__Projection();
    std::vector<Ra__Node__Rename*> renames;

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
            // Ra__Node__Predicate binaryOperator?
            PgQuery__AExpr* a_expr = node->a_expr;
            Ra__Node__Predicate* p = new Ra__Node__Predicate();
            Ra__Node__Expression* ra_l_expr = new Ra__Node__Expression();
            Ra__Node__Expression* ra_r_expr = new Ra__Node__Expression();

            p->binaryOperator=a_expr->name[0]->string->str;

            parse_expression(a_expr->lexpr, ra_l_expr);
            parse_expression(a_expr->rexpr, ra_r_expr);
            p->left = ra_l_expr;
            p->right = ra_r_expr;

            return p;
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
    sel->predicate = parse_where_expression(select_stmt->where_clause);

    return sel;
}

// return subtree with crossproducts & relations
Ra__Node* parse_from(PgQuery__SelectStmt* select_stmt){

    if(select_stmt->n_from_clause==0){
        return nullptr;
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

Ra__Node* parse_query(const char* query){

    std::cout << "Parsing protobuf" << std::endl;

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

        /* SELECT */
        Ra__Node* projections = parse_select(stmt->select_stmt);
        if(projections != nullptr){
            root->childNodes.push_back(projections);
        }
        

        /* WHERE */
        Ra__Node* selections = parse_where(stmt->select_stmt);
        if(selections != nullptr){
            // add selections to bottom of linear subtree
            while(it->childNodes.size()>0){
                it = it->childNodes[0];
            }
            it->childNodes.push_back(selections);
        }

        
        /* FROM */
        Ra__Node* cross_products = parse_from(stmt->select_stmt);
        if(cross_products != nullptr){
            // add cross products to bottom of linear subtree
            while(it->childNodes.size()>0){
                it = it->childNodes[0];
            }
            it->childNodes.push_back(cross_products);
        }

        return root;
    }
}