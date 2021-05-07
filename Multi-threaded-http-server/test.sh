curl -T 0123456789 http://localhost:8080/0123456789 &
curl -T 0123456789 http://localhost:8080/0123456788 &
curl -T 0123456789 http://localhost:8080/1111111111 &
curl -T 0123456789 http://localhost:8080/1000000000 &
curl -T 0123456789 http://localhost:8080/1111111112 &
curl -T 0123456789 http://localhost:8080/0123456788

echo "CHECKING THE PUT"

diff ./copy1/0123456789 0123456789
diff ./copy2/0123456789 0123456789
diff ./copy1/1111111111 0123456789
diff ./copy2/1111111111 0123456789
diff ./copy1/1000000000 0123456789
diff ./copy2/1000000000 0123456789
diff ./copy1/1111111112 0123456789
diff ./copy2/1111111112 0123456789
diff ./copy1/0123456788 0123456789
diff ./copy2/0123456788 0123456789

echo "FINISHED CHECKING THE PUT"

curl http://localhost:8080/0123456789 > testout1.txt &
curl http://localhost:8080/1111111111 > testout2.txt &
curl http://localhost:8080/1000000000 > testout3.txt &
curl http://localhost:8080/0123456788 > testout4.txt &
curl http://localhost:8080/1111111112 > testout5.txt

echo "CHECKING THE GET"

diff testout1.txt ./copy1/0123456789
diff testout2.txt ./copy1/1111111111
diff testout3.txt ./copy1/1000000000
diff testout4.txt ./copy1/0123456788
diff testout4.txt ./copy1/1111111112

echo "FINISHED CHECKING THE GET"