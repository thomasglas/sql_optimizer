# install.packages("ggplot2")
# load package and data
options(scipen=999)  # turn-off scientific notation like 1e+48
library(ggplot2)
theme_set(theme_bw())  # pre-set the bw theme.

##### READ POSTGRES #####
postgres <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\query_throughput\\postgresql.csv')
postgres <- postgres[0:22,]
postgres$query <- factor(postgres$query, levels = postgres$query)
postgres$original <- as.numeric(as.character(postgres$original))
ggplot(postgres, aes(x = query, y = original)) + geom_bar(stat = "identity") +labs(y= "Query runtime", x = "TPCH Query")

##### READ DUCKDB #####
duckdb <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\query_throughput\\duckDB.csv')
duckdb <- duckdb[0:22,]
duckdb$query <- factor(duckdb$query, levels = duckdb$query)
duckdb$original <- as.numeric(as.character(duckdb$original))
ggplot(duckdb, aes(x = query, y = original)) + geom_bar(stat = "identity") +labs(y= "Query runtime", x = "TPCH Query")

##### READ MYSQL #####
mysql <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\query_throughput\\mysql.csv')
mysql <- mysql[0:22,]
mysql$query <- factor(mysql$query, levels = mysql$query)
mysql$original <- as.numeric(as.character(mysql$original))
ggplot(mysql, aes(x = query, y = original)) + geom_bar(stat = "identity") +labs(y= "Query runtime", x = "TPCH Query")

##### READ SQLITE #####
sqlite <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\query_throughput\\sqlite.csv')
sqlite <- sqlite[0:22,]
sqlite$query <- factor(sqlite$query, levels = sqlite$query)
sqlite$original <- as.numeric(as.character(sqlite$original))
ggplot(sqlite, aes(x = query, y = original)) + geom_bar(stat = "identity") +labs(y= "Query runtime", x = "TPCH Query")

