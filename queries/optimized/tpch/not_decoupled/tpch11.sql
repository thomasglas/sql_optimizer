select ps_partkey, sum((ps_supplycost*ps_availqty)) as value
from nation, supplier, partsupp
where ps_suppkey=s_suppkey and s_nationkey=n_nationkey and n_name='GERMANY'
group by ps_partkey
having sum((ps_supplycost*ps_availqty))>(select (sum((ps_supplycost*ps_availqty))*0.0001)
from nation, supplier, partsupp
where ps_suppkey=s_suppkey and s_nationkey=n_nationkey and n_name='GERMANY'
)
order by value desc;
