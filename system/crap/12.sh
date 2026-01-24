tar -czvf anvil_source.tar.gz \
    --exclude='.git' \
    --exclude='node_modules' \
    --exclude='BUILD_TMP' \
    --exclude='ARTIFACTS' \
    --exclude='*.iso' \
    --exclude='*.tar.gz' \
    .
