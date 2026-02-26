extern void register_integration_metadata_tests(void);
extern void register_cli_core_tests(void);
extern void register_cli_ml_similarity_tests(void);

void register_integration_tests(void) {
  register_integration_metadata_tests();
  register_cli_core_tests();
  register_cli_ml_similarity_tests();
}
