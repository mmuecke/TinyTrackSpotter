<template>
  <q-card>
    <q-form @submit="onSubmit">
      <q-input v-model:model-value="clientIdX" label="Client Id" />
      <q-input :model-value="clientSecret" label="Client Secret" />
      <q-input :model-value="accessToken" label="Access Token" />
      {{ tokens }}
      <!-- <q-input v-model="tokens.refresh_token" label="Refresh Token" />
      <q-input v-model="tokens.expires_in" label="Expires in" />
      <q-input v-model="tokens.token_type" label="Token Type" />
      <q-input v-model="tokens.scope" label="Scope" />
      <q-input v-model="tokens.expires_at" label="Expires at" /> -->
      <q-btn label="Submit" type="submit" />
    </q-form>
  </q-card>
</template>

<script setup lang="ts">
import { storeToRefs } from 'pinia';
import { useEspStore } from 'src/stores/espStore';
import { useSpotifyStore } from 'src/stores/spotifyStore';
import { ref } from 'vue';

const store = useEspStore();
const store1 = useSpotifyStore();

const { clientId, clientSecret, accessToken } = storeToRefs(store);
const { tokens } = storeToRefs(store1);

const clientIdX = ref(clientId.value)

function onSubmit () {
  console.log('submit');
  store.setClientId(clientId.value);
}
</script>
