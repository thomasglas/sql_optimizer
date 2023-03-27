select 
        l.l_partkey,
        l.l_suppkey 
from 
        lineitem l, 
        partsupp ps 
where 
        l.l_partkey=ps.ps_partkey 
        and l.l_partkey<20 
        and l.l_quantity=( 
                select 
                        max(l2.l_quantity) 
                from 
                        lineitem l2, 
                        part p 
                where 
                        l2.l_partkey=ps.ps_partkey 
                        and p.p_partkey=ps.ps_partkey 
        );
