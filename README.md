# Encryption-Device-Driver

1 Introduction
In this Repo I implemented an encryption device driver using a kernel module.
The module uses simple XOR cipher to implement an encrypted buffer.
Each device file has an associated in-memory buffer, such that read and write operations can be encrypted or decrypted according to an encryption key.
It uses an 8-bit key (a char), i.e., encrypting each char we write into the device buffer, and decrypting each char we read from the device buffer.
The module stores an in-memory buffer for each device file associated with it. 
Additionally, each open file has its own encryption key (char) and flag, which can be changed via ioctl command.

The module supports the following special operations (ioctl commands):
10. Get major: returns the major number allocated to the kernel module in registration. The command parameter is unused.
20. Set key: sets the encryption key for the current open file. The command parameter is treated as a char (no need for validation).
30. Encrypt: turns encryption on or off, by setting the flag according to the command parameter (0 or 1). If the parameter is not 0 or 1, returns an error -EINVAL.
40. Rewind: sets the current open file's offset to 0. The command id of each operation is its list number.

The module supports the following standard operations: open, release, read, write, ioctl (unlocked ioctl).
Read and write: a read/write operation returns data from the buffer associated with the
current device file. If the encryption flag is off (0), these operations read and write data as usual, operating on the contents of the buffer as requested by the user. If the encryption flag is on (1), these operations encrypt or decrypt the data.
On read, data is read from the device file buffer, decrypted with the XOR key of the current open file, and returned to the user. On write, data in the user buffer is encrypted with the XOR key of the current open file, and the encrypted data is written into the device file buffer.

This code has to be compiled with: "sudo make", using the attached Makefile.
