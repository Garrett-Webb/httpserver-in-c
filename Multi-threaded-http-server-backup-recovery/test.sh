curl -T da_plan.txt http://localhost:8080/0123456788
curl -T 0123456789 http://localhost:8080/0123456787 
curl -T da_plan.txt http://localhost:8080/1111111111 
curl -T makefile http://localhost:8080/1000000000 
curl -T makefile http://localhost:8080/1111111112
curl -T 0123456789 http://localhost:8080/0123456786

curl http://localhost:8080/b

echo "CHECKING THE PUT"

diff da_plan.txt 0123456788
diff 0123456789 0123456787
diff da_plan.txt 1111111111
diff makefile 1000000000
diff makefile 1111111112
diff 0123456789 0123456786

echo "FINISHED CHECKING THE PUT"

curl -T 0123456789 http://localhost:8080/0123456789 
curl -T 0123456789 http://localhost:8080/0123456788 
curl -T 0123456789 http://localhost:8080/1111111111 
curl -T 0123456789 http://localhost:8080/1000000000 
curl -T 0123456789 http://localhost:8080/1111111112
curl -T 0123456789 http://localhost:8080/0123456788

curl http://localhost:8080/r

echo "CHECKING THE RECOVERY"

diff da_plan.txt 0123456788
diff 0123456789 0123456787
diff da_plan.txt 1111111111
diff makefile 1000000000
diff makefile 1111111112
diff 0123456789 0123456786

echo "FINISHED CHECKING THE RECOVERY"
