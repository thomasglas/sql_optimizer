select 
        l.l_partkey, 
        l.l_suppkey
from 
        partsupp ps, 
        lineitem l, (
                select 
                        max(l2.l_quantity), 
                        l2.l_partkey
                from 
                        part p, 
                        lineitem l2
                where 
                        p.p_partkey=l2.l_partkey
                group by 
                        l2.l_partkey
        ) as t1(m,t1_ps_partkey)
where 
        l.l_quantity=t1.m 
        and ps.ps_partkey=t1.t1_ps_partkey 
        and l.l_partkey=ps.ps_partkey 
        and l.l_partkey<20000;
