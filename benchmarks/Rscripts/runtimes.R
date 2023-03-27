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
tpch$query <- factor(tpch$query, levels = c('Q1','Q2','Q3','Q4','Q5','Q6','Q7','Q8','Q9','Q10','Q11','Q12','Q13','Q14','Q15','Q16','Q17','Q18','Q19','Q20','Q21','Q22'))
tpch$original <- as.numeric(as.character(tpch$original))
tpch$decoupled <- as.numeric(as.character(tpch$decoupled))
tpch$not_decoupled <- as.numeric(as.character(tpch$not_decoupled))

p1 <- ggplot(tpch, aes(fill=database, y=original, x=query)) + geom_bar(position="dodge", stat="identity") +labs(y= "Query runtime (s)", x = "TPCH Query")
p1 + guides(fill=guide_legend(title="Database")) + scale_y_cut(breaks=c(31, 5001), which=c(1,1,1), scales=c(2,1,0.5), space=0.25) + scale_y_continuous(breaks=c(10,20,30,500,1000,2000,3000,4000,5000,10000,20000,30000), limits=c(0, 30000)) + theme(legend.position="bottom")

print(tpch)