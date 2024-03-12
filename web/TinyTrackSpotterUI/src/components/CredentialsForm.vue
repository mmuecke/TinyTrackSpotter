<template>
  <q-form @submit="onSubmit" @reset="onReset" class="q-gutter-md">
    <q-input filled v-model="clientId" :label="$t('credentialsForm.clientId.label')"
      :hint="$t('credentialsForm.clientId.hint')" :rules="[
    (val) => (val && val.length > 0) || $t('credentialsForm.rules.required', { name: $t('credentialsForm.clientId.label') }),
    (val) => isHash(val) || $t('credentialsForm.rules.hash', { name: $t('credentialsForm.clientId.label'), length: '32' }),
  ]" />

    <q-input filled v-model="clientSecret" :label="$t('credentialsForm.clientSecret.label')"
      :hint="$t('credentialsForm.clientSecret.hint')" :rules="[
    (val) => (val && val.length > 0) || $t('credentialsForm.rules.required', { name: $t('credentialsForm.clientSecret.label') }),
    (val) => isHash(val) || $t('credentialsForm.rules.hash', { name: $t('credentialsForm.clientId.label'), length: '32' }),
  ]" />

    <div>
      <q-btn :label="$t('credentialsForm.submit')" type="submit" color="primary" />
      <q-btn :label="$t('credentialsForm.reset')" type="reset" color="primary" flat class="q-ml-sm" />
    </div>
  </q-form>
</template>

<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { useSpotifyStore } from 'src/stores/spotifyStore';

const store = useSpotifyStore();
const { clientId, clientSecret } = storeToRefs(store);

function isHash (string: string) {
  return /^[a-f0-9]{32}$/.test(string);
}

async function onSubmit () {
  if (!clientId.value || !clientSecret.value) {
    return;
  }
  // Redirect to Spotify to request authorization
  store.requestAuthorization(clientId.value, clientSecret.value);
}

function onReset () {
  clientId.value = null;
  clientSecret.value = null;
}
</script>
