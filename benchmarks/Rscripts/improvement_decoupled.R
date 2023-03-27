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

tpch_correlated <- tpch[c(0,2,17,20,2+22,17+22,20+22,2+44,17+44,20+44,2+66,17+66,20+66),]
tpch_correlated$a <- with(tpch_correlated, original / decoupled)

print(tpch_correlated)

p1 <- ggplot(tpch_correlated, aes(fill=database, y=a, x=query)) + geom_bar(position="dodge", stat="identity") +labs(y= "Improvement factor", x = "TPCH Query")
p1 + guides(fill=guide_legend(title="Database")) + scale_y_cut(breaks=c(3.1), scales=c(1,1), space=0.25) + scale_y_continuous(breaks=c(1,2,3,100,500,1000,1500,2000,2500,3000,3500), limits=c(0, 3500))+ theme(legend.position="bottom")
