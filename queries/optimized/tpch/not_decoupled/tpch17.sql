with cte_2 as (
	select 
		l_extendedprice, 
		l_quantity, 
		p_partkey, 
		l_partkey
	from 
		part, 
		lineitem
	where 
		p_partkey=l_partkey 
		and (p_brand='Brand#23' and p_container='MED BOX')
)
select 
	(sum(l_extendedprice)/7.0) as avg_yearly
from 
	cte_2, 
	(select 
		(0.2*avg(l_quantity)), 
		d.p_partkey
	from 	(select 
			p_partkey
		from cte_2
		) as d(p_partkey), lineitem
	where l_partkey=d.p_partkey
	group by d.p_partkey
	) as t1(m,t1_p_partkey)
where 
	l_quantity<t1.m 
	and p_partkey=t1.t1_p_partkey;
