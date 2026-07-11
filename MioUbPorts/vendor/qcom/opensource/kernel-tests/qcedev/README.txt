			QCEDEV Test Application
			========================
		      Subsystem: kernel-tests/qcedev
		      ================================
Description:
===========
qcedev driver provides IOCTLS for user space application to access crypto
engine hardware for crypto services.
The test application tests different crypto cipher and hash algorithms
supported by qcedev driver.
The tests are categorized as nominal, adversial, repeatability and stress.
The algorithms that are tested are:
 CIPHER: AES-128 (CBC/ECB/CTR/XTS) and AES-256 (CBC/ECB/CTR/XTS)
 HASH:  SHA1, SHA256 and AES-MAC, CIPHER(AES and DES), CMAC, SHA256.

Running the Test Application:
=============================
- Change directory to data/kernel-tests
	#cd data/kernel-tests
- Change permission for qcedev_test:
	#chmod 777 qcedev_test'
- Run qcedev_test - See Usage section below for the parameters:
	#./qcedev_test -<OPTION> -<TEST_TYPE0> -<TEST_TYPE1> ..-<TEST_TYPEn>

Usage:
======
qcedev_test -[OPTION] -[TEST_TYPE0]..-[TEST_TYPEn]
	Runs the user space tests specified by the TEST_TYPE parameter(s).

	OPTION can be:
	-v, --verbose         run with debug messages on TEST_TYPE can be:

		  -n, --nominal         run standard functionality tests
		  -a, --adversarial     run tests that try to break the
		                          driver
		  -r, --repeatability   run 200 iterations of both the
		                          nominal and adversarial tests
		  -s, --stress          run tests that try to maximize the
		                          capacity of the driver
		  -p, --performance     run cipher and sha256 performance
					  tests.
		  -h, --help            print this help message and exit


