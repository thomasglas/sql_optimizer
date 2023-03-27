CREATE OR REPLACE TABLE nation
(
    n_nationkey  INTEGER not null PRIMARY KEY,
    n_name       CHAR(25) not null,
    n_regionkey  INTEGER not null,
    n_comment    VARCHAR(152)
);

CREATE OR REPLACE TABLE region
(
    r_regionkey  INTEGER not null PRIMARY KEY,
    r_name       CHAR(25) not null,
    r_comment    VARCHAR(152)
);

CREATE OR REPLACE TABLE part
(
    p_partkey     BIGINT not null PRIMARY KEY,
    p_name        VARCHAR(55) not null,
    p_mfgr        CHAR(25) not null,
    p_brand       CHAR(10) not null,
    p_type        VARCHAR(25) not null,
    p_size        INTEGER not null,
    p_container   CHAR(10) not null,
    p_retailprice DOUBLE PRECISION not null,
    p_comment     VARCHAR(23) not null
);

CREATE OR REPLACE TABLE supplier
(
    s_suppkey     BIGINT not null PRIMARY KEY,
    s_name        CHAR(25) not null,
    s_address     VARCHAR(40) not null,
    s_nationkey   INTEGER not null,
    s_phone       CHAR(15) not null,
    s_acctbal     DOUBLE PRECISION not null,
    s_comment     VARCHAR(101) not null
);

CREATE OR REPLACE TABLE partsupp
(
    ps_partkey     BIGINT not null,
    ps_suppkey     BIGINT not null,
    ps_availqty    BIGINT not null,
    ps_supplycost  DOUBLE PRECISION  not null,
    ps_comment     VARCHAR(199) not null,
    PRIMARY KEY(ps_partkey, ps_suppkey)
);

CREATE OR REPLACE TABLE customer
(
    c_custkey     BIGINT not null PRIMARY KEY,
    c_name        VARCHAR(25) not null,
    c_address     VARCHAR(40) not null,
    c_nationkey   INTEGER not null,
    c_phone       CHAR(15) not null,
    c_acctbal     DOUBLE PRECISION   not null,
    c_mktsegment  CHAR(10) not null,
    c_comment     VARCHAR(117) not null
);

CREATE OR REPLACE TABLE orders
(
    o_orderkey       BIGINT not null PRIMARY KEY,
    o_custkey        BIGINT not null,
    o_orderstatus    CHAR(1) not null,
    o_totalprice     DOUBLE PRECISION not null,
    o_orderdate      DATE not null,
    o_orderpriority  CHAR(15) not null,  
    o_clerk          CHAR(15) not null, 
    o_shippriority   INTEGER not null,
    o_comment        VARCHAR(79) not null
);

CREATE OR REPLACE TABLE lineitem
(
    l_orderkey    BIGINT not null,
    l_partkey     BIGINT not null,
    l_suppkey     BIGINT not null,
    l_linenumber  BIGINT not null,
    l_quantity    DOUBLE PRECISION not null,
    l_extendedprice  DOUBLE PRECISION not null,
    l_discount    DOUBLE PRECISION not null,
    l_tax         DOUBLE PRECISION not null,
    l_returnflag  CHAR(1) not null,
    l_linestatus  CHAR(1) not null,
    l_shipdate    DATE not null,
    l_commitdate  DATE not null,
    l_receiptdate DATE not null,
    l_shipinstruct CHAR(25) not null,
    l_shipmode     CHAR(10) not null,
    l_comment      VARCHAR(44) not null,
    PRIMARY KEY (l_orderkey, l_linenumber)
);

INSERT INTO nation SELECT * FROM read_csv_auto('/opt/tpch/sf1/nation.tbl');
INSERT INTO region SELECT * FROM read_csv_auto('/opt/tpch/sf1/region.tbl');
INSERT INTO part SELECT * FROM read_csv_auto('/opt/tpch/sf1/part.tbl');
INSERT INTO supplier SELECT * FROM read_csv_auto('/opt/tpch/sf1/supplier.tbl');
INSERT INTO partsupp SELECT * FROM read_csv_auto('/opt/tpch/sf1/partsupp.tbl');
INSERT INTO customer SELECT * FROM read_csv_auto('/opt/tpch/sf1/customer.tbl');
INSERT INTO orders SELECT * FROM read_csv_auto('/opt/tpch/sf1/orders.tbl');
INSERT INTO lineitem SELECT * FROM read_csv_auto('/opt/tpch/sf1/lineitem.tbl');

// .open tpch
// .open tpch_01
// .open tpch_10

// .timer on