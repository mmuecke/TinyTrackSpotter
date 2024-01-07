<template>
  <q-page class="row items-center justify-evenly">
    <credentials-form />
    <redirect-handler />
  </q-page>
</template>

<script setup lang="ts">
import CredentialsForm from 'src/components/CredentialsForm.vue';
import RedirectHandler from 'src/components/RedirectHandler.vue';
import { useRouter } from 'vue-router';
const $router = useRouter();
console.log($router.currentRoute.value.query)


</script>
