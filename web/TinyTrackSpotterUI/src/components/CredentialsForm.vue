<template>
  <q-form @submit="onSubmit" @reset="onReset" class="q-gutter-md">
    <q-input filled v-model="clientId" :label="$t('credentialsForm.clientId.label')"
      :hint="$t('credentialsForm.clientId.hint')" :rules="[
        (val) => (val && val.length > 0) || $t('credentialsForm.rules.required', { name:  $t('credentialsForm.clientId.label')}),
        (val) => isHash(val) || $t('credentialsForm.rules.hash', { name: $t('credentialsForm.clientId.label'), length: '32' }),
      ]" />

    <q-input filled v-model="clientSecret" :label="$t('credentialsForm.clientSecret.label')"
      :hint="$t('credentialsForm.clientSecret.hint')" :rules="[
        (val) => (val && val.length > 0) || $t('credentialsForm.rules.required', { name: $t('credentialsForm.clientSecret.label')}),
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
// const clientId = ref<string | null>(null);
// const clientSecret = ref<string | null>(null);

function isHash (string: string) {
  return /^[a-f0-9]{32}$/.test(string);
}

function onSubmit () {
  if(!clientId.value || !clientSecret.value) {
    return;
  }
  store.requestAuthorization(clientId.value, clientSecret.value);
  // if (accept.value !== true) {
  //   $q.notify({
  //     color: 'red-5',
  //     textColor: 'white',
  //     icon: 'warning',
  //     message: 'You need to accept the license and terms first'
  //   })
  // }
  // else {
  //   $q.notify({
  //     color: 'green-4',
  //     textColor: 'white',
  //     icon: 'cloud_done',
  //     message: 'Submitted'
  //   })
  // }
}

function onReset () {
  // name.value = null
  // age.value = null
  // accept.value = false
}
</script>
