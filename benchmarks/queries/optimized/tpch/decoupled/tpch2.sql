select 
	s_acctbal, 
	s_name, 
	n_name, 
	p_partkey, 
	p_mfgr, 
	s_address, 
	s_phone, 
	s_comment
from 
	region, 
	nation, 
	partsupp, 
	supplier, 
	part, 
	(select 
		min(ps_supplycost), 
		ps_partkey
	from 
		region, 
		nation, 
		supplier, 
		partsupp
	where 
		s_suppkey=ps_suppkey 
		and s_nationkey=n_nationkey 
		and n_regionkey=r_regionkey 
		and r_name='EUROPE'
	group by ps_partkey
	) as t1(m,t1_p_partkey)
where 
	ps_supplycost=t1.m 
	and p_partkey=t1.t1_p_partkey 
	and p_partkey=ps_partkey 
	and (s_suppkey=ps_suppkey 
	and s_nationkey=n_nationkey) 
	and n_regionkey=r_regionkey 
	and r_name='EUROPE' 
	and (p_size=15 and p_type like '%BRASS')
order by 
	s_acctbal desc, 
	n_name, 
	s_name, 
	p_partkey;
