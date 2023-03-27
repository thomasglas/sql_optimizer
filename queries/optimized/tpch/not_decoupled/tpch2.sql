with cte_2 as (
	select 
		s_acctbal, 
		s_name, 
		n_name, 
		p_partkey, 
		p_mfgr, 
		s_address, 
		s_phone, 
		s_comment, 
		ps_supplycost, 
		ps_partkey, 
		s_suppkey, 
		ps_suppkey, 
		s_nationkey, 
		n_nationkey, 
		n_regionkey, 
		r_regionkey, 
		r_name
	from 
		region, 
		nation, 
		partsupp, 
		supplier, 
		part
	where 
		p_partkey=ps_partkey 
		and (s_suppkey=ps_suppkey 
		and s_nationkey=n_nationkey) 
		and n_regionkey=r_regionkey 
		and r_name='EUROPE' 
		and (p_size=15 and p_type like '%BRASS')
)
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
	cte_2, 	(select min(ps_supplycost), 
			d.p_partkey
		from 	(select p_partkey
			from cte_2
			) as d(p_partkey), 
			region, 
			nation, 
			supplier, 
			partsupp
		where 
			d.p_partkey=ps_partkey 
			and s_suppkey=ps_suppkey 
			and s_nationkey=n_nationkey 
			and n_regionkey=r_regionkey 
			and r_name='EUROPE'
		group by 
			d.p_partkey
		) as t1(m,t1_p_partkey)
where 
	ps_supplycost=t1.m 
	and p_partkey=t1.t1_p_partkey
order by 
	s_acctbal desc, 
	n_name, 
	s_name, 
	p_partkey;
