 

  memset (out, 0x42, sizeof (out));
  len = 0;
  ok = base64_decode (b64in, 4, out, &len);
  ASSERT (ok);
  ASSERT (len == 0);

  memset (out, 0x42, sizeof (out));
  len = 1;
  ok = base64_decode (b64in, 4, out, &len);
  ASSERT (ok);
  ASSERT (len == 1);
  ASSERT (memcmp (out, "abcdefg", 1) == 0);

  memset (out, 0x42, sizeof (out));
  len = 2;
  ok = base64_decode (b64in, 4, out, &len);
  ASSERT (ok);
  ASSERT (len == 2);
  ASSERT (memcmp (out, "abcdefg", 2) == 0);

  memset (out, 0x42, sizeof (out));
  len = 3;
  ok = base64_decode (b64in, 4, out, &len);
  ASSERT (ok);
  ASSERT (len == 3);
  ASSERT (memcmp (out, "abcdefg", 3) == 0);

  memset (out, 0x42, sizeof (out));
  len = 4;
  ok = base64_decode (b64in, 4, out, &len);
  ASSERT (ok);
  ASSERT (len == 3);
  ASSERT (memcmp (out, "abcdefg", 3) == 0);

  memset (out, 0x42, sizeof (out));
  len = 100;
  ok = base64_decode (b64in, strlen (b64in), out, &len);
  ASSERT (ok);
  ASSERT (len == 7);
  ASSERT (memcmp (out, "abcdefg", 7) == 0);

   

  len = base64_encode_alloc (in, strlen (in), &p);
  ASSERT (len == 24);
  ASSERT (strcmp (p, "YWJjZGVmZ2hpamtsbW5vcA==") == 0);
  free (p);

  len = base64_encode_alloc (in, IDX_MAX - 5, &p);
  ASSERT (len == 0);

   
  {
    struct base64_decode_context ctx;

    base64_decode_ctx_init (&ctx);

    len = sizeof (out);
    ok = base64_decode_ctx (&ctx, b64in, strlen (b64in), out, &len);
    ASSERT (ok);
    ASSERT (len == 7);
    ASSERT (memcmp (out, "abcdefg", len) == 0);
  }

   

  ok = base64_decode_alloc_ctx (NULL, b64in, strlen (b64in), &p, &len);
  ASSERT (ok);
  ASSERT (len == 7);
  ASSERT (memcmp (out, "abcdefg", len) == 0);
  free (p);

  {
    struct base64_decode_context ctx;
    const char *newlineb64 = "YWJjZG\nVmZ2hp\namtsbW5vcA==";

    base64_decode_ctx_init (&ctx);

    ok = base64_decode_alloc_ctx (&ctx, newlineb64, strlen (newlineb64), &p, &len);
    ASSERT (ok);
    ASSERT (len == strlen (in));
    ASSERT (memcmp (p, in, len) == 0);
    free (p);
  }

  {
    struct base64_decode_context ctx;
    base64_decode_ctx_init (&ctx);

    ok = base64_decode_alloc_ctx (&ctx, "YW\nJjZGVmZ2hp", 13, &p, &len);
    ASSERT (ok);
    ASSERT (len == 9);
    ASSERT (memcmp (p, "abcdefghi", len) == 0);
    free (p);

    base64_decode_ctx_init (&ctx);

    ok = base64_decode_alloc_ctx (&ctx, "YW\n", 3, &p, &len);
    ASSERT (ok);
    ASSERT (len == 0);
    free (p);

    ok = base64_decode_alloc_ctx (&ctx, "JjZGVmZ2", 8, &p, &len);
    ASSERT (ok);
    ASSERT (len == 6);
    ASSERT (memcmp (p, "abcdef", len) == 0);
    free (p);

    ok = base64_decode_alloc_ctx (&ctx, "hp", 2, &p, &len);
    ASSERT (ok);
    ASSERT (len == 3);
    ASSERT (memcmp (p, "ghi", len) == 0);
    free (p);

    ok = base64_decode_alloc_ctx (&ctx, "", 0, &p, &len);
    ASSERT (ok);
    free (p);
  }

  {
    struct base64_decode_context ctx;
    const char *newlineb64 = "\n\n\n\n\n";

    base64_decode_ctx_init (&ctx);

    ok = base64_decode_alloc_ctx (&ctx, newlineb64, strlen (newlineb64), &p, &len);
    ASSERT (ok);
    ASSERT (len == 0);
    free (p);
  }

  ok = base64_decode_alloc_ctx (NULL, " ! ", 3, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "abc\ndef", 7, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aa", 2, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aa=", 3, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aax", 3, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aa=X", 4, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aa=X", 4, &p, &len);
  ASSERT (!ok);

  ok = base64_decode_alloc_ctx (NULL, "aax=X", 5, &p, &len);
  ASSERT (!ok);

  return 0;
}
