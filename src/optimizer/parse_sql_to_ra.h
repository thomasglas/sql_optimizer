#ifndef parse_sql_to_ra
#define parse_sql_to_ra

#include <pg_query.h>
#include <memory>
#include "protobuf/pg_query.pb-c.h"
#include "relational_algebra.h"
#include "ra_tree.h"

class SQLtoRA{
    public:
        SQLtoRA();

        /**
         * Parses SQL and translates to relational algebra
         * 
         * @param query SQL query
         * @return Pointer to root node of relational algebra tree
         */
        std::shared_ptr<RaTree> parse(const char* query);

    private:
        // to generate unique ids
        uint64_t counter = 0;

        /// Root node of main relational algebra tree
        std::shared_ptr<Ra__Node> ra_tree_root;

        /// Relational algebra trees of Common Table Expressions
        std::vector<std::shared_ptr<Ra__Node>> ctes;

        /**
         * Builds relational algebra tree for "select" statement
         *
         * @param select_stmt Pointer to parsed select statement
         * @return Pointer to root node of relational algebra tree
         */
        std::shared_ptr<Ra__Node> parse_select_statement(PgQuery__SelectStmt* select_stmt);
        
        /**
         * Parses a with clause
         *
         * @param select_stmt pointer to with clause 
         */
        void parse_with(PgQuery__WithClause* with_clause);

        /**
         * Parses a select clause
         *
         * @param select_stmt pointer to select
         * @return Relational algebra subtree with projection
         */
        std::shared_ptr<Ra__Node> parse_select(PgQuery__SelectStmt* select_stmt);

        /**
         * Parses a from clause 
         *
         * @param from_clause Pointer to from clause arguments
         * @param n_from_clause Number of from clause arguments
         * @return Pointer to relational algebra subtree (with relations, cross products, joins)
         */
        std::shared_ptr<Ra__Node> parse_from(PgQuery__Node** from_clause, size_t n_from_clause);

        /**
         * Parses a where clause 
         *
         * @param from_clause Pointer to where clause
         * @return Pointer to relational algebra subtree
         */
        std::shared_ptr<Ra__Node> parse_where(PgQuery__Node* where_clause);

        /**
         * Builds relational algebra tree for "having" clause
         *
         * @param select_stmt Pointer to a relational algebra tree node
         * @param relations Vector which is filled with pointers to all relations
         * @return Pointer to Ra__Node__Having node (may have subquery)
         */
        std::shared_ptr<Ra__Node> parse_having(PgQuery__Node* having_clause);

        /**
         * Builds relational algebra tree for "group by" clause
         *
         * @param group_by_args Pointer to group by arguments
         * @param n_group_by_args Number of group by arguments
         * @return Pointer to Ra__Node__Group_By node
         */
        std::shared_ptr<Ra__Node> parse_group_by(PgQuery__Node** group_clause, size_t n_group_clause);

        /**
         * Builds relational algebra tree for "order by" clause
         *
         * @param sort_clause Pointer to order by arguments
         * @param n_sort_clause Number of order by arguments
         * @return Pointer to Ra__Node__Order_By node
         */
        std::shared_ptr<Ra__Node> parse_order_by(PgQuery__Node** sort_clause, size_t n_sort_clause);

        /**
         * Finds first empty leaf in relational algebra tree (DFS)
         *
         * @param it Pointer to a relational algebra node
         * @return True if empty leaf found, else false
         */
        bool find_empty_leaf(std::shared_ptr<Ra__Node>& it);

        /**
         * Attaches a relational algebra subtree to an existing relational algebra tree, at first empty leaf found (DFS)
         *
         * @param base Pointer to the base relational algebra tree
         * @param subtree Pointer to a relational algebra subtree to be attached to base
         */
        void add_subtree(std::shared_ptr<Ra__Node> base, std::shared_ptr<Ra__Node> subtree);

        /**
         * Parses a join expression (in "from" clause) 
         *
         * @param join_expr Pointer to the join expression
         * @return Pointer to Ra__Node__Join node 
         */
        std::shared_ptr<Ra__Node> parse_from_join(PgQuery__JoinExpr* join_expr);

        // parses whole where expression, returns predicate for selection
        // adds childnodes to selection if needed
        /**
         * Parses a where expression 
         *
         * @param node Pointer to where expression
         * @param ra_selection Selection node belonging to the parent where clause. 
         *      If where expression contains subqueries, their subtrees are attached to selection node.
         * @param sublink_negated True if current expression is a subquery (exists, in) that is negated with "not"
         * @return Pointer to predicate, to be added to parent selection node
         */
        std::shared_ptr<Ra__Node> parse_where_expression(PgQuery__Node* node, std::shared_ptr<Ra__Node> ra_selection, bool sublink_negated=false);

        /**
         * Finds all attributes used in a where expression
         *
         * @param node Pointer to where expression
         * @param attributes Vector of attributes, filled with any attributes found 
         */
        void find_where_expression_attributes(PgQuery__Node* node, std::vector<std::shared_ptr<Ra__Node__Attribute>>& attributes);

        /**
         * Checks whether a subquery is correlated or not
         *
         * @param select_stmt Pointer to select statement
         * @return True if subquery is correlated, else false
         */
        bool is_correlated_subquery(PgQuery__SelectStmt* select_stmt);

        /**
         * Checks whether an attribute is an attribute belonging to a tpch relation
         *
         * @param attr name of attribute
         * @param name of relation
         * @return True if relation is tpch and attribute belongs to relation
         */
        bool is_tpch_attribute(std::string attr, std::string relation);

        /**
         * Parses an "in" subquery
         *
         * @param sub_link pointer to in subquery
         * @param negated if the exists subquery is negated
         * @return Relational algebra subtree with join node and subquery on one side of join
         */
        std::shared_ptr<Ra__Node> parse_where_in_subquery(PgQuery__SubLink* sub_link, bool negated);

        /**
         * Parses an "in" list expression
         *
         * @param l_expr pointer to left expression of "in" 
         * @param r_expr pointer to right expression of "in" (list) 
         * @param negated if the in subquery is negated
         * @return Relational algebra subtree with join node and subquery on one side of join
         */
        std::shared_ptr<Ra__Node> parse_where_in_list(PgQuery__Node* r_expr);

        /**
         * Parses an exists subquery
         *
         * @param select_stmt pointer to select statement of subquery
         * @param negated if the exists subquery is negated
         * @return Relational algebra subtree with join node and subquery on one side of join
         */
        std::shared_ptr<Ra__Node> parse_where_exists_subquery(PgQuery__SelectStmt* select_stmt, bool negated);

        /**
         * Parses a subquery in where expression of type: "x = subquery"
         *
         * @param select_stmt pointer to select statement of subquery
         * @param ra_arg pointer to predicate of the subquery side, to be assigned
         * @return Relational algebra subtree with join node and subquery on one side of join
         */
        std::shared_ptr<Ra__Node> parse_where_subquery(PgQuery__SelectStmt* select_stmt, std::shared_ptr<Ra__Node>& ra_arg);

        /**
         * Parses a subquery in from clause
         *
         * @param range_subselect pointer to subquery
         * @return Relational algebra subtree with projection
         */
        std::shared_ptr<Ra__Node> parse_from_subquery(PgQuery__RangeSubselect* range_subselect);

        /**
         * Finds all attributes referenced in an expression (E.g. min(s.id) -> s.id)
         *
         * @param node pointer to expression
         * @param attributes vector of attributes, filled with all attributes found
         */
        void find_expression_attributes(PgQuery__Node* node, std::vector<std::shared_ptr<Ra__Node__Attribute>>& attributes);

        /**
         * Parses an expression
         *
         * @param node pointer to expression
         * @param ra_arg Pointer to relational algebra node, to be assigned with the parsed expression
         * @param has_aggregate Flag which is set to true, if aggregating expression is found
         */
        void parse_expression(PgQuery__Node* node, std::shared_ptr<Ra__Node>& ra_arg, bool& has_aggregate);
};

#endif