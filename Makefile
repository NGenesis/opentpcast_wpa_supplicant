all:
	g++ -O3 -std=c++14 -Wall -pedantic -c opentpcast_wpa_supplicant.cpp -o opentpcast_wpa_supplicant.o
	g++ -s -o opentpcast_wpa_supplicant opentpcast_wpa_supplicant.o

clean:
	rm -f opentpcast_wpa_supplicant *.o
