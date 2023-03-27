with cte_2 as (
        select 
                l.l_partkey, 
                l.l_suppkey, 
                l.l_quantity, 
                ps.ps_partkey
        from 
                partsupp ps, 
                lineitem l
        where 
                l.l_partkey=ps.ps_partkey 
                and l.l_partkey<20
)
select 
        cte_2.l_partkey, 
        cte_2.l_suppkey
from 
        cte_2, (
                select 
                        max(l2.l_quantity), 
                        d.ps_partkey
                from (
                        select 
                                ps_partkey
                        from 
                                cte_2
                        ) as d(ps_partkey), 
                        part p, 
                        lineitem l2
                where 
                        l2.l_partkey=d.ps_partkey 
                        and p.p_partkey=d.ps_partkey
                group by 
                        d.ps_partkey
                ) as t1(m,t1_ps_partkey)
where cte_2.l_quantity=t1.m and cte_2.ps_partkey=t1.t1_ps_partkey;
