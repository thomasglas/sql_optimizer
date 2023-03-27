# install.packages("ggplot2")
# load package and data
options(scipen=999)  # turn-off scientific notation like 1e+48
library(ggplot2)
theme_set(theme_bw())  # pre-set the bw theme.

tpch <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\postgresql.csv')
print(tpch)
tpch <- tpch[0:22,]
tpch$query <- factor(tpch$query, levels = tpch$query)
tpch$original.2 <- as.numeric(as.character(tpch$original.2))

tpch_original <- tpch[0:22,1:5]
tpch_decoupled <- tpch[0:22,6:9]
tpch_not_decoupled <- tpch[0:22,10:13]

# plot original runtimes
ggplot(tpch_original, aes(x = query, y = original.2)) + geom_bar(stat = "identity") + scale_y_log10() +labs(y= "Query runtime", x = "TPCH Query")

# plot improvement of decoupled vs original 
tpch_correlated <- tpch[c(0,2,4,17,20,21,22),]
tpch_correlated$a <- with(tpch_correlated, original.2 / decoupled.2)
ggplot(tpch_correlated, aes(x = query, y = a)) + geom_bar(stat = "identity") + scale_y_continuous(trans = scales::pseudo_log_trans(sigma = 0.1), breaks = 10^(0:5)) +labs(y= "Query runtime improvement", x = "TPCH Query")  +labs(y= "Query runtime improvement", x = "TPCH Query") +
  ylim(0, 3000)

# plot improvement of not-decoupled vs decoupled 
tpch$b <- with(tpch, decoupled.2 / not_decoupled.2)
ggplot(tpch, aes(x = query, y = b)) + geom_bar(stat = "identity")
