# install.packages("ggplot2")
# load package and data
options(scipen=999)  # turn-off scientific notation like 1e+48
library(ggplot2)
library(ggbreak) 
library(cowplot) 
library(ggplotify) 
library(ggimage) 
theme_set(theme_bw())  # pre-set the bw theme.

##### READ RUNTIMES #####
tpch <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\runtimes.csv')
# tpch$query <- factor(tpch$query, levels = tpch$query)
tpch$query <- factor(tpch$query, levels = c('Q2','Q4','Q17','Q20','Q21','Q22'))
tpch$original <- as.numeric(as.character(tpch$original))
tpch$decoupled <- as.numeric(as.character(tpch$decoupled))
tpch$not_decoupled <- as.numeric(as.character(tpch$not_decoupled))

tpch_correlated <- tpch[c(0,4,21,22,4+22,21+22,22+22,4+44,21+44,22+44,4+66,21+66,22+66),]
tpch_correlated$a <- with(tpch_correlated, original / decoupled)

print(tpch_correlated)

p1 <- ggplot(tpch_correlated, aes(fill=database, y=a, x=query)) + geom_bar(position="dodge", stat="identity") +labs(y= "Improvement factor", x = "TPCH Query")
p1 + guides(fill=guide_legend(title="Database")) + scale_y_continuous(breaks=c(0.2,0.4,0.6,0.8,1.0,1.2,1.4,1.6,1.8,2.0), limits=c(0, 2))+ theme(legend.position="bottom")
