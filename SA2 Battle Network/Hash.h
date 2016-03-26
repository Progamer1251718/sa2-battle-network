#pragma once

#include <WinCrypt.h>
#include <vector>
#include <string>

class Hash
{
public:	
	/// <summary>
	/// Initializes a new instance of the <see cref="Hash"/> class.
	/// </summary>
	/// <param name="dwProvType">Provider type. See "Cryptographic Provider Types" on MSDN for details.</param>
	/// <param name="dwFlags">See the MSDN definition of CryptAcquireContext for details.</param>
	Hash(DWORD dwProvType = PROV_RSA_AES, DWORD dwFlags = CRYPT_VERIFYCONTEXT);
	~Hash();

	std::vector<uchar> ComputeHash(void* data, size_t size, ALG_ID kind) const;
	static std::string toString(std::vector<uchar>& hash);
	static std::vector<uchar> fromString(const std::string& str);

private:
	HCRYPTPROV csp;
};
