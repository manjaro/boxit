#include "cryptsha256.h"


QString CryptSHA256::sha256CheckSum(QString filePath) {
    QByteArray checksum;
    FILE *f;
    int i, j;
    sha256_context ctx;
    unsigned char buf[1000];
    unsigned char sha256sum[32];

    if(!(f = fopen(filePath.toUtf8().data(), "rb")))
        return QString("");

    sha256_starts( &ctx );

    while( ( i = fread( buf, 1, sizeof( buf ), f ) ) > 0 )
    {
        sha256_update( &ctx, buf, i );
    }

    fclose(f);

    sha256_finish( &ctx, sha256sum );

    for( j = 0; j < 32; j++ )
        checksum.append(sha256sum[j]);

    return QString(checksum.toHex());
}
