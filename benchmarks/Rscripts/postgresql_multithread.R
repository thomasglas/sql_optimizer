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
tpch <- read.csv('C:\\Users\\thoma\\Documents\\Uni\\sql_optimizer\\benchmarks\\postgresql_multithread2.csv')
tpch$query <- factor(tpch$query, levels = c('Q1','Q2','Q3','Q4','Q5','Q6','Q7','Q8','Q9','Q10','Q11','Q12','Q13','Q14','Q15','Q16','Q17','Q18','Q19','Q20','Q21','Q22'))
tpch$original <- as.numeric(as.character(tpch$original))
tpch$best_decorrelated <- as.numeric(as.character(tpch$best_decorrelated))
tpch$a <- with(tpch, original / best_decorrelated)

print(tpch$best_decorrelated)

p1 <- ggplot(tpch, aes(group=Threads, y=a, x=query, fill=Threads)) + geom_bar(position="dodge", stat="identity") +labs(y= "Improvement factor", x = "TPCH Query")
p1 + guides(fill=guide_legend(title="max_parallel_workers_per_gather"))+ scale_y_cut(breaks=c(11), which=c(1,2), scales=c(1,1,1), space=0.25) + scale_y_continuous(breaks=c(1,5,10,500,1000,1500,2000,2500,3000,3500,4000,4500,5000), limits=c(0, 5000))+ theme(legend.position="bottom")
