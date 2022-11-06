#ifndef ra_tree
#define ra_tree

#include "relational_algebra.h"

class RaTree {
    public:
        RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes);
        ~RaTree();

        /// Root node of main relational algebra tree
        Ra__Node* root;

        /// Relational algebra trees of Common Table Expressions
        std::vector<Ra__Node*> ctes;
};

#endif