// Welcome to the easiest way to parse an SQL query :-)
// Compile the file like this:
//
// g++ -g -o deparse -I../ -I../src/postgres/include/ -I../vendor/ -L../ deparse.cc -lpg_query -pthread

#include <pg_query.h>
#include <stdio.h>
#include <stdlib.h>
#include "protobuf/pg_query.pb-c.h"
#include "jsoncpp.cpp"
#include "relational_algebra.h"

size_t testCount = 1;
const char* tests[] = {
  "SELECT s.name, s.id from students s where s.id=1",
  "SELECT e.grade from exams e, students s where e.sid=s.id"
};

void parse_json(){

  PgQueryParseResult result;
  size_t i;

  for (i = 0; i < testCount; i++) {
    result = pg_query_parse(tests[i]);

    if (result.error) {
      printf("error: %s at %d\n", result.error->message, result.error->cursorpos);
    } else {
      printf("%s\n", result.parse_tree);
    }
    
    std::istringstream iss(result.parse_tree);
    Json::Value root;
    iss >> root;
    std::string encoding = root["version"].asString();
    std::cout << encoding << std::endl;
    // printf("%s\n", root.get("version", "UTF-8").asCString());
    std::cout << root <<std::endl;
    pg_query_free_parse_result(result);
  }
}

void parse_deparse_query(){

  PgQueryProtobufParseResult result;
  size_t i;

  for (i = 0; i < testCount; i++) {
    result = pg_query_parse_protobuf(tests[i]);

    PgQueryDeparseResult deparsed_result = pg_query_deparse_protobuf(result.parse_tree);
    printf("%s\n", deparsed_result.query);

    pg_query_free_deparse_result(deparsed_result);
    pg_query_free_protobuf_parse_result(result);
  }
}
  
void printRaTree(Projection* pr){

  std::string select = "select ";
  for(auto attr: pr->attributes){
    select += attr->attribute[0] + "." + attr->attribute[1] + ", ";
  }
  select.pop_back();
  select.pop_back();

  std::string from = "from ";
  Selection* sel = static_cast<Selection*>(pr->innerNode);
  Relations* rel = static_cast<Relations*>(sel->innerNode);
  for(auto relation: rel->relations){
      from += relation[0] + " " + relation[1] + ", ";
  }
  from.pop_back();
  from.pop_back();

  
  std::string where = "where ";
  where += sel->p->left->toString() + sel->p->binaryOperator + sel->p->right->toString();

  std::cout << select << "\n" << from << "\n" << where << "\n" << std::endl;
}

void parse_protobuf(){

  printf("Parsing protobuf:\n");

  for(auto test: tests){
    PgQueryProtobufParseResult result;  
    result = pg_query_parse_protobuf(test);
    PgQuery__ParseResult* parse_result;
    parse_result = pg_query__parse_result__unpack(NULL, result.parse_tree.len, (const uint8_t*) result.parse_tree.data);
    
    for(size_t i=0; i<parse_result->n_stmts; i++){
      PgQuery__RawStmt* raw_stmt = parse_result->stmts[i];
      PgQuery__Node* stmt = raw_stmt->stmt;
      PgQuery__Node__NodeCase node_case = stmt->node_case;
      PgQuery__SelectStmt* select_stmt = stmt->select_stmt;

      // go through target_list, from_clause, where_clause
      // create projection
      Projection* pr = new Projection();
      RaNode* innerNode = pr;

      // add selected attributes to projection
      for(size_t j=0; j<select_stmt->n_target_list; j++){
        PgQuery__Node* target = select_stmt->target_list[j];
        PgQuery__ResTarget* res_target = target->res_target;
        PgQuery__ColumnRef* columnRef = res_target->val->column_ref;
        Attribute* attr = new Attribute();
        for(size_t k=0; k<columnRef->n_fields; k++){
          char* ref = columnRef->fields[k]->string->str;
          attr->attribute.push_back(ref);
        }
        pr->attributes.push_back(attr);
      }

      // if where clause exists, create selection
      if(select_stmt->where_clause!=nullptr){
        Selection* sel = new Selection();
        PgQuery__AExpr* expr = select_stmt->where_clause->a_expr;
        Predicate* p = new Predicate();
        
        // left expression
        if(expr->lexpr->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_COLUMN_REF){
          PgQuery__ColumnRef* columnRef = expr->lexpr->column_ref;
          Attribute* attr = new Attribute();
          for(size_t k=0; k<columnRef->n_fields; k++){
            char* ref = columnRef->fields[k]->string->str;
            attr->attribute.push_back(ref);
          }
          p->left = new PredicateLeaf(attr);
        }
        else{
          //is const
          Constant* constValue;
          if(expr->lexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_INTEGER){
            constValue = new Constant(expr->lexpr->a_const->val->integer->ival);
          }
          else if(expr->lexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_FLOAT){
            constValue = new Constant(std::stof(expr->lexpr->a_const->val->float_->str));
          }
          else if(expr->lexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_STRING){
            constValue = new Constant(expr->lexpr->a_const->val->string->str);
          }
          else{
            std::cout << "error 5" << std::endl;
            constValue=nullptr;
          }
          p->left = new PredicateLeaf(constValue);
        }

        // right expression
        if(expr->rexpr->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_COLUMN_REF){
          PgQuery__ColumnRef* columnRef = expr->rexpr->column_ref;
          Attribute* attr = new Attribute();
          for(size_t k=0; k<columnRef->n_fields; k++){
            char* ref = columnRef->fields[k]->string->str;
            attr->attribute.push_back(ref);
          }
          p->right = new PredicateLeaf(attr);
        }
        else{
          //is const
          Constant* constValue;
          if(expr->rexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_INTEGER){
            constValue = new Constant(expr->rexpr->a_const->val->integer->ival);
          }
          else if(expr->rexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_FLOAT){
            constValue = new Constant(std::stof(expr->rexpr->a_const->val->float_->str));
          }
          else if(expr->rexpr->a_const->val->node_case==PgQuery__Node__NodeCase::PG_QUERY__NODE__NODE_STRING){
            constValue = new Constant(expr->rexpr->a_const->val->string->str);
          }
          else{
            std::cout << "error 5" << std::endl;
            constValue=nullptr;
          }
          p->right = new PredicateLeaf(constValue);
        }

        // add binary operator
        PgQuery__Node* name = *(expr->name);
        p->binaryOperator = name->string->str;

        sel->p = p;
        pr->innerNode = sel;
        innerNode=sel;
      }

      // add from clause relations
      std::vector<std::vector<std::string>> relations;
      for(size_t j=0; j<select_stmt->n_from_clause; j++){
        std::vector<std::string> relation;
        PgQuery__Node* from_clause = select_stmt->from_clause[j];
        PgQuery__RangeVar* from_range_var = from_clause->range_var;
        relation.push_back(from_range_var->relname);
        relation.push_back(from_range_var->alias->aliasname);
        relations.push_back(relation);
      }
      Selection* sel = (Selection*)innerNode;
      sel->innerNode = new Relations(relations);

      printRaTree(pr);
    }
  }
}

void generate_protobuf(){
  // create a new protobuf and deparse it
  // final product: PgQueryProtobuf
  // PgQueryProtobuf = packed PgQuery__ParseResult 

  // PgQueryProtobuf* result;
  // PgQuery__ParseResult* parse_result;
  // pg_query__parse_result__init(parse_result);
  // parse_result->stmts

  // pg_query::ParseResult parseResult;

}

int main() {
  parse_protobuf();
  // Optional, this ensures all memory is freed upon program exit (useful when running Valgrind)
  pg_query_exit();

  return 0;
}
