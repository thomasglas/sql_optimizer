select 
	(sum(l_extendedprice)/7.0) as avg_yearly
from 
	part, 
	lineitem, 
	(select (0.2*avg(l_quantity)), 
		l_partkey
	from 
		lineitem
	group by 
		l_partkey
	) as t1(m,t1_p_partkey)
where 
	l_quantity<t1.m 
	and p_partkey=t1.t1_p_partkey 
	and p_partkey=l_partkey 
	and (p_brand='Brand#23' and p_container='MED BOX');
