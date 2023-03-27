with cte_4 as (
	select ps_suppkey, ps_partkey, ps_availqty
	from partsupp
)
select s_name, s_address
from nation, supplier
where 
	s_suppkey in 
	(select 
		ps_suppkey
	from 
		cte_4, 
		(select 
			(0.5*sum(l_quantity)), 
			d.ps_partkey, 
			d.ps_suppkey
		from 
			(select 
				ps_partkey, 
				ps_suppkey
			from cte_4
			) as d(ps_partkey,ps_suppkey), 
			lineitem
		where 
			l_partkey=d.ps_partkey 
			and l_suppkey=d.ps_suppkey 
			and (l_shipdate>=date '1994-01-01' 
			and l_shipdate<(date '1994-01-01'+interval '1' year))
		group by 
			d.ps_partkey, 
			d.ps_suppkey
		) as t3(m,t3_ps_partkey,t3_ps_suppkey)
	where ps_partkey in 
		(select p_partkey
		from part
		where p_name like 'forest%'
		) 
		and (ps_availqty>t3.m 
		and ps_partkey=t3.t3_ps_partkey 
		and ps_suppkey=t3.t3_ps_suppkey)
		) and s_nationkey=n_nationkey 
		and n_name='CANADA'
order by s_name;
