AUTHOR="supercop-20221122/crypto_sign/ed25519/ref/implementors"
FILES="
	supercop-20221122/crypto_verify/32/ref/verify.c
	supercop-20221122/crypto_sign/ed25519/ref/fe25519.h
	supercop-20221122/crypto_sign/ed25519/ref/fe25519.c
	supercop-20221122/crypto_sign/ed25519/ref/sc25519.h
	supercop-20221122/crypto_sign/ed25519/ref/sc25519.c
	supercop-20221122/crypto_sign/ed25519/ref/ge25519.h
	supercop-20221122/crypto_sign/ed25519/ref/ge25519.c
	supercop-20221122/crypto_sign/ed25519/ref/keypair.c
	supercop-20221122/crypto_sign/ed25519/ref/sign.c
	supercop-20221122/crypto_sign/ed25519/ref/open.c
"
DATA="supercop-20221122/crypto_sign/ed25519/ref/ge25519_base.data"
set -e
cd $1
echo -n '/*  $'
echo 'OpenBSD: $ */'
echo
echo '/*'
echo ' * Public Domain, Authors:'
sed -e '/Alphabetical order:/d' -e 's/^/ * - /' < $AUTHOR
echo ' */'
echo
echo '
echo
echo '
echo
for t in int8 uint8 int16 uint16 int32 uint32 int64 uint64; do
	echo "
done
echo
for i in $FILES; do
	echo "/* from $i */"
	sed \
	    -e "/
	    -e "/
	    -e "s/^void /static void /g" \
	    -e 's/CRYPTO_NAMESPACE[(]\([a-zA-Z0-9_]*\)[)]/crypto_sign_ed25519_ref_\1/g' \
	    $i | \
	case "$i" in
	*/crypto_verify/32/ref/verify.c)
	    sed -e "/^
	        -e "s/crypto_verify/crypto_verify_32/g" \
	        -e "s/^int /static int /g"
	    ;;
	*/crypto_sign/ed25519/ref/sign.c)
	    sed -e "s/crypto_sign/crypto_sign_ed25519/g"
	    ;;
	*/crypto_sign/ed25519/ref/keypair.c)
	    sed -e "s/crypto_sign_keypair/crypto_sign_ed25519_keypair/g"
	    ;;
	*/crypto_sign/ed25519/ref/open.c)
	    sed -e "s/crypto_sign_open/crypto_sign_ed25519_open/g"
	    ;;
	*/crypto_sign/ed25519/ref/fe25519.*)
	    sed -e "s/reduce_add_sub/fe25519_reduce_add_sub/g" \
	        -e "s/ equal[(]/ fe25519_equal(/g" \
	        -e "s/^int /static int /g"
	    ;;
	*/crypto_sign/ed25519/ref/sc25519.h)
	    sed -e "s/^int /static int /g" \
	        -e '/shortsc25519_from16bytes/d' \
	        -e '/sc25519_iszero_vartime/d' \
	        -e '/sc25519_isshort_vartime/d' \
	        -e '/sc25519_lt_vartime/d' \
	        -e '/sc25519_sub_nored/d' \
	        -e '/sc25519_mul_shortsc/d' \
	        -e '/sc25519_from_shortsc/d' \
	        -e '/sc25519_window5/d'
	    ;;
	*/crypto_sign/ed25519/ref/sc25519.c)
	    sed -e "s/reduce_add_sub/sc25519_reduce_add_sub/g" \
	        -e "s/ equal[(]/ sc25519_equal(/g" \
	        -e "s/^int /static int /g" \
	        -e "s/m[[]/sc25519_m[/g" \
	        -e "s/mu[[]/sc25519_mu[/g" \
	        -e '/shortsc25519_from16bytes/,/^}$/d' \
	        -e '/sc25519_iszero_vartime/,/^}$/d' \
	        -e '/sc25519_isshort_vartime/,/^}$/d' \
	        -e '/sc25519_lt_vartime/,/^}$/d' \
	        -e '/sc25519_sub_nored/,/^}$/d' \
	        -e '/sc25519_mul_shortsc/,/^}$/d' \
	        -e '/sc25519_from_shortsc/,/^}$/d' \
	        -e '/sc25519_window5/,/^}$/d'
	    ;;
	*/crypto_sign/ed25519/ref//ge25519.*)
	    sed -e "s/^int /static int /g"
	    ;;
	*)
	    cat
	    ;;
	esac | \
	sed -e 's/[	 ]*$//'
done
