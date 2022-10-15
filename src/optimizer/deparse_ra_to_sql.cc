#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <map>
#include <cassert>

#include "deparse_ra_to_sql.h"

std::string generate_select(Ra__Node* node, std::map<std::string,std::string> rename_map){
    std::string result = "";
    auto projection = static_cast<Ra__Node__Projection*>(node);

    for(auto expression: projection->expressions){
        assert(expression->consts_attributes.size()>0);

        size_t i = 0;
        do{
            if(i>0){
                switch(expression->operators[i-1]){
                    case RA__ARITHMETIC_OPERATOR__PLUS: {
                        result += "+"; 
                        break;
                    }
                    case RA__ARITHMETIC_OPERATOR__MINUS: {
                        result += "-"; 
                        break;
                    }
                    case RA__ARITHMETIC_OPERATOR__MULTIPLY: {
                        result += "*"; 
                        break;
                    }
                    case RA__ARITHMETIC_OPERATOR__DIVIDE: {
                        result += "/"; 
                        break;
                    }
                }
            }
            switch(expression->consts_attributes[i]->node_case){
                case RA__NODE__CONST: {
                    Ra__Node__Constant* constant = static_cast<Ra__Node__Constant*>(expression->consts_attributes[i]);
                    result += constant->to_string();
                    break;
                }
                case RA__NODE__ATTRIBUTE: {
                    Ra__Node__Attribute* attr = static_cast<Ra__Node__Attribute*>(expression->consts_attributes[i]);
                    result += attr->to_string();
                    break;
                }
                default: std::cout << "error deparse select" << std::endl;
            }
            i++;
        }while(i<expression->consts_attributes.size());
        
        result += ", ";
    }
    result.pop_back();
    result.pop_back();
    return result;
}

std::string generate_where(Ra__Node* node){
    return "";
}

std::string generate_from(Ra__Node* node){
    return "";
}

void ra_tree_dfs(Ra__Node* node, std::string& select, std::string& where, std::string& from, std::map<std::string,std::string>& rename_map){

    switch(node->node_case){
        case RA__NODE__SELECTION: {
            where += generate_where(node) + " and ";
            break;
        }
        case RA__NODE__PROJECTION: {
            select += generate_select(node, rename_map) + ", ";
            break;
        }
        case RA__NODE__RENAME: {
            auto rename = static_cast<Ra__Node__Rename*>(node);
            rename_map[rename->old_to_string()]=rename->new_to_string();
            break;
        }
        case RA__NODE__RELATION: {
            from += generate_from(node) + ", ";
            break;
        }
        default: {
            // nothing to do
            break;
        }
    }
    for(auto child: node->childNodes){
        // recurse: 
        ra_tree_dfs(child, select, where, from, rename_map);
    }
}

std::string deparse_ra(Ra__Node* ra_root){

    // return "printing!";
    std::map<std::string,std::string> rename_map;
    std::string select = "";
    std::string from = "";
    std::string where = "";

    ra_tree_dfs(ra_root, select, where, from, rename_map);

    select.pop_back();
    select.pop_back();

    std::string sql = "select " + select;
    if(from.length()>0){
        sql += "from " + from;
    }
    if(where.length()>0){
        sql += "where " + where;
    }

    return sql;
    // each rename we find -> add to map
    // if find projection -> generate select statement
        // if find selection -> generate where statement
        // if find cross product/relation -> generate from statement

    // std::string select = "select ";
    // for(auto attr: pr->attributes){
    //     select += attr->attribute[0] + "." + attr->attribute[1] + ", ";
    // }
    // select.pop_back();
    // select.pop_back();

//   std::string from = "from ";
//   Selection* sel = static_cast<Selection*>(pr->innerNode);
//   Relations* rel = static_cast<Relations*>(sel->innerNode);
//   for(auto relation: rel->relations){
//       from += relation[0] + " " + relation[1] + ", ";
//   }
//   from.pop_back();
//   from.pop_back();

  
//   std::string where = "where ";
//   where += sel->p->left->toString() + sel->p->binaryOperator + sel->p->right->toString();

//   std::cout << select << "\n" << from << "\n" << where << "\n" << std::endl;
}
