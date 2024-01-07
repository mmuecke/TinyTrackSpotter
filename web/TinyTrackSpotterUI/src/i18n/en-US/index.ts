// This is just an example,
// so you can safely delete all default props below

export default {
  failed: 'Action failed',
  success: 'Action was successful',
  credentialsForm: {
    clientId: {
      label: 'Client ID',
      hint: 'Enter client ID',
    },
    clientSecret: {
      label: 'Client Secret',
      hint: 'Enter client secret',
    },
    rules: {
      required: '{name} is required',
      hash: '{name} must be a {length} char hash',
    },
    submit: 'Request Authorization',
    reset: 'Reset',
  },
};
