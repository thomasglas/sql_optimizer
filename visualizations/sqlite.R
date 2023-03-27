# install.packages("ggplot2")
# load package and data
# options(scipen=999)  # turn-off scientific notation like 1e+48
library(ggplot2)
theme_set(theme_bw())  # pre-set the bw theme.

tpch <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\sqlite.csv')
print(tpch)
tpch <- tpch[0:22,]
tpch$query <- factor(tpch$query, levels = tpch$query)
tpch$original <- as.numeric(as.character(tpch$original))
tpch$decoupled <- as.numeric(as.character(tpch$decoupled))
tpch$not_decoupled <- as.numeric(as.character(tpch$not_decoupled))

# plot original runtimes
ggplot(tpch, aes(x = query, y = original)) + geom_bar(stat = "identity") + labs(y= "Query runtime", x = "TPCH Query") + scale_y_continuous(trans = scales::pseudo_log_trans(sigma = 0.1), breaks = 10^(0:5))

# plot improvement of decoupled vs original 
tpch_correlated <- tpch[c(0,2,4,17,20,21,22),]
tpch_correlated$a <- with(tpch_correlated, original / not_decoupled)
ggplot(tpch_correlated, aes(x = query, y = a)) + geom_bar(stat = "identity") + scale_y_continuous(trans = scales::pseudo_log_trans(sigma = 0.1), breaks = 10^(0:5)) +labs(y= "Query runtime improvement", x = "TPCH Query") +
  ylim(0, 4500)
