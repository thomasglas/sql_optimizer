#ifndef deparse_ra_to_sql
#define deparse_ra_to_sql

#include <string>
#include "relational_algebra.h"
#include "ra_tree.h"

class RAtoSQL{
    public:
        RAtoSQL(RaTree* _raTree);
        ~RAtoSQL();

        /**
         * Deparses relational algebra tree to SQL
         * 
         * @return SQL statement as string
         */
        std::string deparse();

    private:
        /// root node of relational algebra tree
        RaTree* raTree;

        /**
         * Deparses a relational algebra projection node to a SQL select statement
         * 
         * @param ra_root pointer to root of relational algebra tree
         * @return SQL select statement as string
         */
        std::string deparse_ctes(std::vector<Ra__Node*>);

        /**
         * Deparses a relational algebra projection node to a SQL select statement
         * 
         * @param ra_root pointer to root of relational algebra tree
         * @return SQL select statement as string
         */
        std::string deparse_projection(Ra__Node* node);

        /**
         * Deparses a relational algebra tree node to SQL. Recursively called on all children
         * 
         * @param node pointer to relational algebra node
         * @param layer Subquery layer of node, 0 for main query
         * @param select SQL select output string
         * @param where SQL where output string
         * @param from SQL from output string
         * @param group_by SQL group by output string
         * @param having SQL having output string
         * @param order_by SQL order by output string
         */
        void deparse_ra_node(Ra__Node* node, size_t layer, std::string& select, std::string& where, std::string& from, std::string& group_by, std::string& having, std::string& order_by);

        /**
         * Deparses relation to SQL
         * 
         * @param node relation
         * @return Relation as SQL string
         */
        std::string deparse_relation(Ra__Node* node);
        
        /**
         * Deparses selection to SQL
         * 
         * @param node selection
         * @return selection as SQL string
         */
        std::string deparse_selection(Ra__Node* node);

        /**
         * Deparses predicate to SQL
         * 
         * @param node predicate
         * @return predicate as SQL string
         */
        std::string deparse_predicate(Ra__Node* node);

        /**
         * Deparses list of expressions to SQL
         * 
         * @param expressions vector of expressions
         * @return expressions as comma separated SQL string
         */
        std::string deparse_expressions(std::vector<Ra__Node*>& expressions);

        /**
         * Deparses list of order by expressions to SQL
         * 
         * @param expressions vector of expressions
         * @return expressions as comma separated SQL string
         */
        std::string deparse_order_by_expressions(std::vector<Ra__Node*>& expressions, std::vector<Ra__Order_By__SortDirection>& directions);

        /**
         * Deparses an expression to SQL
         * 
         * @param expression expression
         * @return expression as SQL string
         */
        std::string deparse_expression(Ra__Node* arg);

        /**
         * Finds the join with corresponding subquery marker, sets it = join when found
         * 
         * @param it pointer to node to search
         * @param marker the marker to be found
         * @return if marker is found
         */
        bool find_marker_subquery(Ra__Node** it, Ra__Node__Where_Subquery_Marker* marker);
};

#endif